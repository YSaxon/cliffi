#include "argparser.h"
#include "types_and_utils.h"
#include <getopt.h>
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

 void addArgToFunctionCallInfo(FunctionCallInfo* info, ArgInfo* arg) {
    if (!info || !arg) return;
    if (!info->args) {
        info->args = malloc(sizeof(ArgInfo*));
        if (!info->args) return;
        info->args[0] = *arg;
        info->arg_count = 1;
    } else {
        info->args = realloc(info->args, (info->arg_count + 1) * sizeof(ArgInfo*));
        if (!info->args) return;
        info->args[info->arg_count] = *arg;
        info->arg_count++;
    }
 }

 

FunctionCallInfo* parse_arguments(int argc, char* argv[]) {
    FunctionCallInfo* info = malloc(sizeof(FunctionCallInfo));
         //createFunctionCallInfo(); // Handle initialization
    int opt;
    char* argStr;

    // arg[1] is the library path
    info->library_path = strdup(argv[1]);
    // arg[2] is the return type
    info->return_type = charToType(argv[2][0]);
    // info->return_type->explicitType = true;

    // arg[3] is the function name
    info->function_name = strdup(argv[3]);
    //TODO: maybe at some point we should be able to take a hex offset instead of a function name

    //TODO: check arguments validity
    argc -= 3;
    argv += 3;


    while ((opt = getopt(argc, argv, "c:h:i:l:C:H:I:L:f:d:s:p:v:")) != -1) {
    ArgType explicitType = TYPE_UNKNOWN;
        switch (opt) {
            case TYPE_CHAR: explicitType = TYPE_CHAR; break;
            case TYPE_SHORT: explicitType = TYPE_SHORT; break;
            case TYPE_INT: explicitType = TYPE_INT; break;
            case TYPE_LONG: explicitType = TYPE_LONG; break;
            case TYPE_UCHAR: explicitType = TYPE_UCHAR; break;
            case TYPE_USHORT: explicitType = TYPE_USHORT; break;
            case TYPE_UINT: explicitType = TYPE_UINT; break;
            case TYPE_ULONG: explicitType = TYPE_ULONG; break;
            case TYPE_FLOAT: explicitType = TYPE_FLOAT; break;
            case TYPE_DOUBLE: explicitType = TYPE_DOUBLE; break;
            case TYPE_STRING: explicitType = TYPE_STRING; break;
            case TYPE_POINTER: explicitType = TYPE_POINTER; break;
            // case TYPE_VOID: explicitType = TYPE_VOID; break; Not allowed for a function argument

            // Handle other type flags
            default: break; // Or handle unknown flags
        }

        if (explicitType != TYPE_UNKNOWN) {
            ArgInfo arg = {.type = explicitType, .explicitType = true};
            convert_arg_value(&arg, optarg);
            addArgToFunctionCallInfo(info, &arg); // Implement this addition
            explicitType = TYPE_UNKNOWN; // Reset for the next argument
        }
    }

    // Handle non-flagged (inferred type) arguments
    for (int index = optind; index < argc; index++) {
        argStr = argv[index];
        ArgInfo arg = {.type = infer_arg_type(argStr), .explicitType = false};
        convert_arg_value(&arg, argStr);
        addArgToFunctionCallInfo(info, &arg);
    }

    return info;
}
