#include "argparser.h"
#include "invoke_handler.h"
#include "library_path_resolver.h"
#include "main.h"
#include "return_formatter.h"
#include "types_and_utils.h"
#include "var_map.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "exception_handling.h"

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
        info->args = malloc(sizeof(void*));
        if (!info->args) return;
        // Set the first element of the array to point to the new ArgInfo struct
        info->args[0] = arg;
        info->arg_count = 1;
    } else {
        // Reallocate memory to accommodate one more ArgInfo struct
        info->args = realloc(info->args, (info->arg_count + 1) * sizeof(void*));;
        // Copy arg to the end of the array
        info->args[info->arg_count] = arg;
        info->arg_count++;
    }
}

void parse_arg_type_from_flag(ArgInfo* arg, const char* argStr){
    int pointer_depth = 0;
    int array_value_pointer_depth = 0;
    ArgType explicitType = charToType(argStr[0]); // Convert flag to type
    while (explicitType == TYPE_POINTER) {
        pointer_depth++;
        explicitType = charToType(argStr[pointer_depth]);
    }
        if (explicitType == TYPE_STRUCT) {
            arg->struct_info = calloc(1, sizeof(StructInfo));
            arg->struct_info->is_packed = false;
            if (argStr[1+pointer_depth] == 'K'){ // SK: = pacKed struct
                arg->struct_info->is_packed = true;
            }
            if (argStr[1 + pointer_depth + arg->struct_info->is_packed] != ':') {
                raiseException(1,  "Error: Struct flag must be followed by a colon and then a list of arguments, in flags %s\n", argStr);
            }
        }


        if (explicitType == TYPE_ARRAY) {
        // We'll need to figure out how to move forward argv past the array values
        // For now we'll just say that the array values can't have a space in them
        // So the entire array will just be one argv
        // In the future we may switch to using end delimitters eg a: 3 2 1 :a
        while (charToType(argStr[1+pointer_depth+array_value_pointer_depth]) == TYPE_POINTER) {
            array_value_pointer_depth++;
        }

        if (isAllDigits(&argStr[1+pointer_depth+array_value_pointer_depth]) || argStr[1+pointer_depth+array_value_pointer_depth] == 't') {
            //argStr[:2+pointer_depth] is the array flag + 'i' + argStr[2+pointer_depth:]
            raiseException(1,  "Error: Array flag must be followed by a primitive type flag, and THEN it can be followed by a size or size_t argnum, so for instance ai4 or ait1 for an array of ints, whereas you have %s\n", argStr);
        }

        explicitType = charToType(argStr[1 + pointer_depth+array_value_pointer_depth]);

        if (explicitType == TYPE_STRUCT){
            raiseException(1,  "Error: Arrays of structs are not presently supported, in flags %s\n", argStr);
        } else if (explicitType == TYPE_UNKNOWN) {
            raiseException(1,  "Error: Unsupported argument type flag in flags %s\n", argStr);
        } else if (explicitType == TYPE_ARRAY) {
            raiseException(1,  "Error: Array types cannot be nested, in flags %s\n", argStr);
        } else if (explicitType == TYPE_POINTER) {
            raiseException(1,  "Error: Array flag in unsupported position in flags %s. Order must be -[p[p..]][a][primitive type flag]\n", argStr);
        }



        // see if there's a size appended to the end of the array flag
        if (isAllDigits(&argStr[2 + pointer_depth+array_value_pointer_depth])){
            arg->is_array = ARRAY_STATIC_SIZE;
            arg->static_or_implied_size = atoi(argStr + 2 + pointer_depth+array_value_pointer_depth);
        } else if (argStr[2 + pointer_depth+array_value_pointer_depth] == 't') {
            // t is a flag indicating that the array size is specified as a size_t in another argument
            if (isAllDigits(&argStr[3 + pointer_depth+array_value_pointer_depth])){
                arg->is_array = ARRAY_SIZE_AT_ARGNUM;
                arg->array_sizet_arg.argnum_of_size_t_to_be_replaced = atoi(argStr + 3 + pointer_depth + array_value_pointer_depth);
            } else {
                //maybe in the future we'll allow just assuming its the next arg if it's not a number
                raiseException(1,  "Error: Array size flag t must be followed by a number in flags %s\n", argStr);
            }
        } else {
            arg->is_array = ARRAY_STATIC_SIZE_UNSET;
            // will be set from the values given later on if it's an argument (or will fail if it's a return)
        }



    }


    if (explicitType == TYPE_UNKNOWN) {
        raiseException(1,  "Error: Unsupported argument type flag in flags %s\n", argStr);
    }
    else if (/*explicitType == TYPE_ARRAY ||*/ explicitType == TYPE_POINTER) {
        raiseException(1,  "Error: Array or Pointer flag in unsupported position in flags %s. Order must be -[p[p..]][a][primitive type flag]\n", argStr);
    }

    arg->type = explicitType;
    arg->explicitType = true;
    arg->pointer_depth = pointer_depth;
    arg->array_value_pointer_depth = array_value_pointer_depth;
}

void parse_all_from_argvs(ArgInfoContainer* info, int argc, char* argv[], int *extra_args_used, bool is_return, bool is_struct);

bool is_type_flag(char* argStr){
    return (argStr[0] == '-' && !isAllDigits(argStr+1) && !isHexFormat(argStr+1)) || strcmp(argStr, ":S") == 0;
    // we should probably also check for NS: etc
}

ArgInfo* parse_one_arg(int argc, char* argv[], int *extra_args_used, bool is_return){
        char* argStr = argv[0];

        ArgInfo* storedVar = getVar(argStr);
        if (storedVar != NULL) {
            return storedVar;
        }

        ArgInfo* outArg = calloc(1,sizeof(ArgInfo));
        outArg->value = malloc(sizeof(*outArg->value));
        bool set_to_null = false;

        int i = 0;
        if (is_return){
            int has_flag = (argStr[0]=='-');
            if (!has_flag) fprintf(stderr, "Warning: dashless type indicators are deprecated : %s\n",argStr);
            parse_arg_type_from_flag(outArg, argStr + has_flag);
        } else if (is_type_flag(argStr)) {
            parse_arg_type_from_flag(outArg, argStr+1);
            if (outArg->type != TYPE_STRUCT) {
                if (i+1<argc && !is_type_flag(argv[i+1])) argStr = argv[++i]; // Set the value to one arg past the flag, and increment i to skip the value
                else {
                    set_to_null = true;
                }
                }
        } else if (argStr[0] == 'N'){ //for NULL
            set_to_null = true;
            parse_arg_type_from_flag(outArg, argStr+1);
            // if (outArg->type != TYPE_STRUCT) argStr = "NULL";
        } else if (argStr[0] == 'O'){ //for NULL
            set_to_null = true;
            parse_arg_type_from_flag(outArg, argStr+1);
            if (!outArg->is_array && !outArg->pointer_depth) { // should this also check for P types?
                raiseException(1,"Error: Outpointer flag is only defined for pointer types: %s",argStr);
            }
            outArg->is_outPointer=true;
        } else { // no flag, so we need to infer the type from the value
            infer_arg_type_from_value(outArg, argStr);
        }

        if (outArg->type==TYPE_STRUCT){ //parsing now in case it's a null type being used for casting a variable
            StructInfo* struct_info = outArg->struct_info; // it's allocated inside parse_arg_type_from_flag
            outArg->struct_info->info.return_var = calloc(1,sizeof(ArgInfo));
            int struct_args_used = 0;
            i++; // skip the S: open tag
            parse_all_from_argvs(&struct_info->info, argc-i, argv+i, &struct_args_used, is_return || set_to_null, true);

            i+=struct_args_used;
            outArg->struct_info = struct_info;
            // not setting a value here, that will be handled by the make_raw_value_for_struct function in the invoke handler module

            if ((set_to_null && !is_return) && getVar(argv[i+1])){
                    argStr = argv[++i]; // just setting for the purpose of checking if the next string afterwards is a variable
            }
            else {
                argStr = "";
            }
            // one problem with this is that we could end up with ambiguity between signatures of ( struct, var ) and ( var cast to struct )
            // but we can fix that by only allowing casting to NULL structs (specified like NS:). So if its an actual struct we don't try to cast to it
        }

        if(!is_return && outArg->explicitType && (outArg->type!=TYPE_STRUCT || set_to_null)){
            // if the value is a variable, we should cast it to the type specified in the flag and return
            ArgInfo* storedVar = getVar(argStr);
            if (storedVar != NULL) {
                fprintf(stdout, "Attempting cast from variable %s of type ", argStr);
                format_and_print_arg_type(storedVar);
                fprintf(stdout, " to type ");
                format_and_print_arg_type(outArg);
                fprintf(stdout, "\n");
                castArgValueToType(outArg, storedVar);
                goto set_args_used_and_return;
            }
        }

        if (outArg->type!=TYPE_STRUCT && (set_to_null || is_return)) { // should probably instead make another field for is_null in both parse_one_arg and parse_all_from_argvs, but this is adequate for now
            set_arg_value_nullish(outArg); // I'm not sure if this causes a memory leak, but fixing memory leaks will be a project for another time
        } else if (!is_return && outArg->type!=TYPE_STRUCT) {
            convert_arg_value(outArg, argStr);
        }
        set_args_used_and_return: // this is a goto label
        *extra_args_used += i;
        return outArg;
}

void parse_all_from_argvs(ArgInfoContainer* info, int argc, char* argv[], int *extra_args_used, bool is_return, bool is_struct) {
    // ArgInfoContainer* arginfo = info->type == FUNCTION_INFO ? &info->function_info->info : &info->struct_info->info;
    #ifdef DEBUG
    fprintf(stdout, "Beginning to parse args (%d remaining)\n", argc);
    #endif
    bool hit_struct_close = false;
    info->vararg_start = -1;
    int i;
    for (i=0; i < argc; i++) {
        char* argStr = argv[i];

        if (strcmp(argStr, ":S") == 0){ // this terminates a struct flag
            if (!is_struct){
                raiseException(1,  "Error: Unexpected close struct flag :S\n");
            }
            hit_struct_close = true;
            break;
        }

        if (strcmp(argStr, "...") == 0) {
            if (info->vararg_start != -1){
                raiseException(1,  "Error: Multiple varargs flags encountered\n");
            } else if (is_return){
                raiseException(1,  "Error: Varargs flag encountered in return type\n");
            } else if (is_struct){
                raiseException(1,  "Error: Varargs flag encountered in struct\n");
            }
            info->vararg_start = info->arg_count;
            continue;
        }

        ArgInfo* arg = parse_one_arg(argc-i, argv+i, &i, is_return);

        addArgToFunctionCallInfo(info, arg);
    }

    if (is_struct && !hit_struct_close){
        raiseException(1,  "Error: Struct flag not closed with :S\n");
    }

    *extra_args_used = i;

    convert_all_arrays_to_arginfo_ptr_sized_after_parsing(info);

    second_pass_arginfo_ptr_sized_null_array_initialization(info);
}


FunctionCallInfo* parse_arguments(int argc, char* argv[]) {
    FunctionCallInfo* info = calloc(1, sizeof(FunctionCallInfo)); // using calloc to zero out the struct

    setCodeSectionForSegfaultHandler("parse_arguments : resolve library path");
    // arg[1] is the library path
    info->library_path = resolve_library_path(argv[0]);
    if (!info->library_path) {
        raiseException(1,  "Error: Unable to resolve library path for %s\n", argv[0]);
    }

    setCodeSectionForSegfaultHandler("parse_arguments : parse return type");
    int args_used_by_return = 0;
    info->info.return_var = parse_one_arg(argc-1, argv+1, &args_used_by_return, true);
    argc-=args_used_by_return;
    argv+=args_used_by_return;

    if (info->info.return_var->type == TYPE_UNKNOWN) {
        raiseException(1,  "Error: Unknown Return Type\n");
        return NULL;
    }
    //check if return is an array without a specified size
    if (info->info.return_var->is_array==ARRAY_STATIC_SIZE_UNSET) {
        raiseException(1,  "Error: Array return types must have a specified size. Put a number at the end of the flag with no spaces, eg %s4 for a static size, or t and a number to specify an argnumber that will represent size_t for it eg %st1 for the first arg, since 0=return\n", argv[1],argv[1]);
    }

    // arg[2] is the function name
    info->function_name = strdup(argv[2]);
    if (!info->function_name) {
        fprintf(stderr, "Error: Unable to allocate memory for function name\n");
        return NULL;
    }

    setCodeSectionForSegfaultHandler("parse_arguments : parse function arguments");
    //TODO: maybe at some point we should be able to take a hex offset instead of a function name
    parse_all_from_argvs(&info->info, argc-3, argv+3, &args_used_by_return, false, false);
    if (args_used_by_return != argc-3){
        raiseException(1,  "Error: Not all arguments were used in parsing\n");
    }
    unsetCodeSectionForSegfaultHandler();

    return info;
}