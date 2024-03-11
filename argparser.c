#include "argparser.h"
#include "invoke_handler.h"
#include "library_path_resolver.h"
#include "types_and_utils.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Function to allocate and initialize a new ArgInfo struct
// ArgInfo* create_arg_info(ArgType type, const char* value) {
//     ArgInfo* arg = malloc(sizeof(ArgInfo));
//     if (!arg) return NULL;
//     arg->type = type;
//     // Omitting detailed value assignment for brevity
//     return arg;
// }

void addArgToFunctionCallInfo(ArgInfoContainer* info, ArgInfo* arg) {
    if (!info || !arg) return;
    if (!info->args) {
        // Allocate memory for a single ArgInfo struct
        info->args = malloc(sizeof(ArgInfo));
        if (!info->args) return;
        // Copy the first ArgInfo struct into the allocated space
        info->args[0] = *arg;
        info->arg_count = 1;
    } else {
        // Reallocate memory to accommodate one more ArgInfo struct
        ArgInfo* temp = realloc(info->args, (info->arg_count + 1) * sizeof(ArgInfo));
        if (!temp) return; // Handle reallocation failure
        info->args = temp;
        // Copy the new ArgInfo struct into the newly allocated space
        info->args[info->arg_count] = *arg;
        info->arg_count++;
    }
}

void parse_arg_type_from_flag(ArgInfo* arg, const char* argStr){
    int pointer_depth = 0;
    ArgType explicitType = charToType(argStr[0]); // Convert flag to type
    while (explicitType == TYPE_POINTER) {
        pointer_depth++;
        explicitType = charToType(argStr[pointer_depth]);
    }
        if (explicitType == TYPE_ARRAY) {
        // We'll need to figure out how to move forward argv past the array values
        // For now we'll just say that the array values can't have a space in them
        // So the entire array will just be one argv
        // In the future we may switch to using end delimitters eg a: 3 2 1 :a
        explicitType = charToType(argStr[1 + pointer_depth]);

        if (isAllDigits(&argStr[1+pointer_depth]) || argStr[1+pointer_depth] == 't') {
            //argStr[:2+pointer_depth] is the array flag + 'i' + argStr[2+pointer_depth:]
            fprintf(stderr, "Error: Array flag must be followed by a primitive type flag, and THEN it can be followed by a size or size_t argnum, so for instance ai4 or ait1 for an array of ints, whereas you have %s\n", argStr);
            exit(1);
        }

        // see if there's a size appended to the end of the array flag
        if (isAllDigits(&argStr[2 + pointer_depth])){
            arg->is_array = ARRAY_STATIC_SIZE;
            arg->array_size.static_size = atoi(argStr + 2 + pointer_depth);
        } else if (argStr[2 + pointer_depth] == 't')
        { 
            // t is a flag indicating that the array size is specified as a size_t in another argument
            if (isAllDigits(&argStr[3 + pointer_depth])){
                arg->is_array = ARRAY_SIZE_AT_ARGNUM;
                arg->array_size.argnum_of_size_t_to_be_replaced = atoi(argStr + 3 + pointer_depth);
            } else {
                //maybe in the future we'll allow just assuming its the next arg if it's not a number
                fprintf(stderr, "Error: Array size flag t must be followed by a number in flags %s\n", argStr);
                exit(1);
            }
        } else if (explicitType == TYPE_STRUCT){
            if (argStr[2 + pointer_depth] != ':') {
                fprintf(stderr, "Error: Struct flag must be followed by a colon (and then the struct fields and then :S ) %s\n", argStr);
                exit(1);
            }
        }
        else {
            arg->is_array = ARRAY_STATIC_SIZE_UNSET;
            // will be set from the values given later on if it's an argument (or will fail if it's a return)
        }

        

    }


    if (explicitType == TYPE_UNKNOWN) {
        fprintf(stderr, "Error: Unsupported argument type flag in flags %s\n", argStr);
        exit(1);
    }
    else if (/*explicitType == TYPE_ARRAY ||*/ explicitType == TYPE_POINTER) {
        fprintf(stderr, "Error: Array or Pointer flag in unsupported position in flags %s. Order must be -[p[p..]][a][primitive type flag]\n", argStr);
        exit(1);
    }

    arg->type = explicitType;
    arg->explicitType = true;
    arg->pointer_depth = pointer_depth;
}

void parse_all_from_argvs(ArgInfoContainer* info, int argc, char* argv[], int *args_used, bool is_return, bool is_struct) {
    // ArgInfoContainer* arginfo = info->type == FUNCTION_INFO ? &info->function_info->info : &info->struct_info->info;
    printf("Beginning to parse args (%d remaining)\n", argc);
    bool hit_struct_close = false;
    int i;
    for (i=0; i < argc; i++) {
        char* argStr = argv[i];
        ArgInfo arg = {0};
        int pointer_depth = 0;

        if (strcmp(argStr, ":S") == 0){ // this terminates a struct flag
            if (!is_struct){
                fprintf(stderr, "Error: Unexpected close struct flag :S\n");
                exit(1);
            }
            hit_struct_close = true;
            break;
        }

        if (is_return){ // is a return type, so we don't need to parse values or check for the - flag
            parse_arg_type_from_flag(&arg, argStr);
            // addArgToFunctionCallInfo(info, &arg);
            // continue;
        } else if (argStr[0] == '-' && !isAllDigits(argStr+1) && !isHexFormat(argStr+1)) {
            parse_arg_type_from_flag(&arg, argStr+1);
            // parse_struct_from_flag(&info->return_var, argv[2]);

            if (arg.type != TYPE_STRUCT) argStr = argv[++i]; // Set the value to one arg past the flag, and increment i to skip the value
        
        } else {
            infer_arg_type_from_value(&arg, argStr);
        }
        if (!is_return && arg.type!=TYPE_STRUCT) {
            convert_arg_value(&arg, argStr);
        } else if (arg.type==TYPE_STRUCT){
            StructInfo* struct_info = calloc(1, sizeof(StructInfo));
            int struct_args_used = 0;
            i++; // skip the S: open tag
            printf("-S tag encountered, parsing struct from args\n");
            parse_all_from_argvs(&struct_info->info, argc-i, argv+i, &struct_args_used, is_return, true);

            i+=struct_args_used;
            arg.struct_info = struct_info;
            // not setting a value here, that will be handled by the make_raw_value_for_struct function in the invoke handler module

        }
        addArgToFunctionCallInfo(info, &arg);
    }

    if (is_struct && !hit_struct_close){
        fprintf(stderr, "Error: Struct flag not closed with :S\n");
        exit(1);
    }

    *args_used = i;

    convert_all_arrays_to_arginfo_ptr_sized_after_parsing(info);

    second_pass_arginfo_ptr_sized_null_array_initialization(info);    
}

 
FunctionCallInfo* parse_arguments(int argc, char* argv[]) {
    FunctionCallInfo* info = calloc(1, sizeof(FunctionCallInfo)); // using calloc to zero out the struct
    int opt;
    char* argStr;

    // arg[1] is the library path
    info->library_path = resolve_library_path(argv[1]);
    if (!info->library_path) {
        fprintf(stderr, "Error: Unable to resolve library path for %s\n", argv[1]);
        return NULL;
    }

    parse_arg_type_from_flag(&info->info.return_var, argv[2]);

    if (info->info.return_var.type == TYPE_UNKNOWN) {
        fprintf(stderr, "Error: Unknown Return Type\n");
        exit(1);
        return NULL;
    } else if (info->info.return_var.type == TYPE_STRUCT) {
        StructInfo* struct_info = calloc(1, sizeof(StructInfo));
        int struct_args_used = 0;
        // i++; // skip the S: open tag
        printf("S tag encountered, parsing struct from args\n");
        parse_all_from_argvs(&struct_info->info, argc-3, argv+3, &struct_args_used, true, true);

        info->info.return_var.struct_info = struct_info;

        argc-=struct_args_used + 1;
        argv+=struct_args_used + 1;
        //not necessary to loop through the pointer depth here, that will be handled by the make_raw_value_for_struct function
    }
    // parse_struct_from_flag(&info->return_var, argv[2]);
    //check if return is an array without a specified size
    if (info->info.return_var.is_array==ARRAY_STATIC_SIZE_UNSET) {
        fprintf(stderr, "Error: Array return types must have a specified size. Put a number at the end of the flag with no spaces, eg %s4 for a static size, or t and a number to specify an argnumber that will represent size_t for it eg %st1 for the first arg, since 0=return\n", argv[2],argv[2]);
        exit(1);
    }

    if (info->info.return_var.type == TYPE_UNKNOWN) {
        fprintf(stderr, "Error: Unsupported return type\n");
        return NULL;
    }

    // arg[3] is the function name
    info->function_name = strdup(argv[3]);
    if (!info->function_name) {
        fprintf(stderr, "Error: Unable to allocate memory for function name\n");
        return NULL;
    }
    //TODO: maybe at some point we should be able to take a hex offset instead of a function name
    int args_used = 0;
    parse_all_from_argvs(&info->info, argc-4, argv+4, &args_used, false, false);
    if (args_used != argc-4){
        fprintf(stderr, "Error: Not all arguments were used in parsing\n");
        exit(1);
    }
    
    return info;
}