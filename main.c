#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "library_path_resolver.h"
#include "argparser.h"
#include "invoke_handler.h"
#include "return_formatter.h"



int main(int argc, char* argv[]) {
    if (strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s <library> <return_type> <function_name> [<args>...]\n", argv[0]);
        printf("  [--help]         Print this help message\n");
        printf("  <library>        The path to the shared library containing the function to invoke\n"
               "                   or the name of the library if it is in the system path\n");
        printf("  <return_type>    The type of the return value of the function to invoke\n"
               "                   v for void, i for int, s for string, etc\n");
        printf("  <function_name>  The name of the function to invoke\n");
        printf("  [typeflag] <arg> The argument values to pass to the function\n"
               "                   Types will be inferred if not prefixed with flags\n"
               "                   Flags look like -i for int, -s for string, etc\n");
        printf("\n"
               "  Examples:\n"
               "         %s libexample.so i addints 3 4\n", argv[0]);
        printf("         %s path/to/libexample.so v dofoo\n", argv[0]);
        printf("         %s ./libexample.so s concatstrings -s hello -s world\n", argv[0]);
        printf("         %s libexample.so s concatstrings hello world\n", argv[0]);
        printf("         %s libexample.so d multdoubles -d 1.5 1.5d\n", argv[0]);
        printf("         %s libc.so i printf 'Here is a number: %%.3f' 4.5", argv[0]);
        return 0;
    }
    else if (argc < 4) {
        fprintf(stderr, "Usage: %s [--help] <library> <return_type> <function_name> [[-typeflag] <arg>...]\n", argv[0]);
        return 1;
    }

    // Step 1: Resolve the library path
    // For now we've delegated that call to parse_arguments


    // Step 2: Parse command-line arguments
    FunctionCallInfo* call_info = parse_arguments(argc, argv);
    ArgInfo retval = {.explicitType = true, .type = call_info->return_type};

    // Step 2.5 (optional): Print the parsed function call info
    printf("Parsed function call info:\n");
    log_function_call_info(call_info);

    // Step 3: Invoke the specified function
    int invoke_result = invoke_dynamic_function(call_info, &retval);
    if (invoke_result != 0) {
        fprintf(stderr, "Error: Function invocation failed\n");
    }

    format_and_print_return_value(&retval);

    // Clean up
    freeFunctionCallInfo(call_info); 

    return invoke_result;
}
