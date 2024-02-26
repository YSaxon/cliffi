#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "library_path_resolver.h"
#include "argparser.h"
#include "invoke_handler.h"
#include "return_formatter.h"



int main(int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <library> <return_type> <function_name> [<args>...]\n", argv[0]);
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
