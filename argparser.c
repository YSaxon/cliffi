#include "argparser.h"
#include "library_path_resolver.h"
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

 

FunctionCallInfo* parse_arguments(int argc, char* argv[]) {
    FunctionCallInfo* info = malloc(sizeof(FunctionCallInfo));
         //createFunctionCallInfo(); // Handle initialization
    int opt;
    char* argStr;

    // arg[1] is the library path
    info->library_path = resolve_library_path(argv[1]);
    if (!info->library_path) {
        fprintf(stderr, "Error: Unable to resolve library path for %s\n", argv[1]);
        return NULL;
    }
    else {
        printf("Library path: %s\n", info->library_path);
    }

    // arg[2] is the return type
    info->return_type = charToType(argv[2][0]);
    // info->return_type->explicitType = true;

    // arg[3] is the function name
    info->function_name = strdup(argv[3]);
    //TODO: maybe at some point we should be able to take a hex offset instead of a function name

    for (int i = 4; i < argc; i++) {
        char* argStr = argv[i];
        ArgInfo arg;

        if (argStr[0] == '-') {
            ArgType explicitType = charToType(argStr[1]); // Convert flag to type
            argStr = argv[++i]; // Move to the next argument
            arg.type = explicitType;
            arg.explicitType = true;
        } else {
            ArgType implicitType = infer_arg_type(argStr);
            arg.type = implicitType;
            arg.explicitType = false;
        }

        printf("Converting Arg %d: %s\n", i - 3, argStr);
        convert_arg_value(&arg, argStr);
        addArgToFunctionCallInfo(info, &arg);
    }

    return info;
}
