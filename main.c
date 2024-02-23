#include <stdio.h>
#include <stdlib.h>
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
    char* library_path = resolve_library_path(argv[1]);
    if (!library_path) {
        fprintf(stderr, "Error: Unable to resolve library path for %s\n", argv[1]);
        return 1;
    }

    // Update the library path in argv for consistent parsing
    argv[1] = library_path;

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
