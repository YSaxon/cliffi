#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "library_path_resolver.h"
#include "argparser.h"
#include "invoke_handler.h"
#include "return_formatter.h"
#include "types_and_utils.h"

const char* NAME = "cliffi";
const char* VERSION = "0.5.7";

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf( "%s %s\n", NAME, VERSION);
        printf( "Usage: %s <library> <typeflag> <function_name> [<args>...]\n", argv[0]);
        printf( "  [--help]         Print this help message\n");
        printf( "  <library>        The path to the shared library containing the function to invoke\n"
                "                   or the name of the library if it is in the system path\n");
        printf( "  <typeflag>       The type of the return value of the function to invoke\n"
                "                   v for void, i for int, s for string, etc\n");
        printf( "  <function_name>  The name of the function to invoke\n");
        printf( "  [-<typeflag>] <arg> The argument values to pass to the function\n"
                "                   Types will be inferred if not prefixed with flags\n"
                "                   Flags look like -i for int, -s for string, etc\n");
        printf( "\n"
                "  BASIC EXAMPLES:\n"
                "         %s libexample.so i addints 3 4\n", argv[0]);
        printf( "         %s path/to/libexample.so v dofoo\n", argv[0]);
        printf( "         %s ./libexample.so s concatstrings -s hello -s world\n", argv[0]);
        printf( "         %s libexample.so s concatstrings hello world\n", argv[0]);
        printf( "         %s libexample.so d multdoubles -d 1.5 1.5d\n", argv[0]);
        printf( "         %s libc.so i printf 'Here is a number: %%.3f' 4.5", argv[0]);
        printf( "\n");
        printf( "  TYPES:\n"
                "     The primitive typeflags are:\n");
        printf( "       %c for void, only allowed as a return type, and does not accept prefixes\n", TYPE_VOID);
        printf( "       %c for char\n", TYPE_CHAR);
        printf( "       %c for short\n", TYPE_SHORT);
        printf( "       %c for int\n", TYPE_INT);
        printf( "       %c for long\n", TYPE_LONG);
        printf( "       %c for unsigned char\n", TYPE_UCHAR);
        printf( "       %c for unsigned short\n", TYPE_USHORT);
        printf( "       %c for unsigned int\n", TYPE_UINT);
        printf( "       %c for unsigned long\n", TYPE_ULONG);
        printf( "       %c for float, can also be specified by suffixing the value with f\n", TYPE_FLOAT);
        printf( "       %c for double, can also be specified by suffixing the value with d\n", TYPE_DOUBLE);
        printf( "       %c for cstring (ie null terminated char*)\n", TYPE_STRING);
        printf("\n");
        printf( "  POINTERS AND ARRAYS AND STRUCTS:\n"
                "     Typeflags can include additional flag prefixes to specify pointers, arrays or structs:\n"
                "     <typeflag> = [p[p..]]<primitive_type>\n"
                "     <typeflag> = [p[p..]][a<size>|t<argnum>]<primitive_type>\n"
                "     <typeflag> = [p[p..]]S: <arg> <arg> ... :S \n"
                "   POINTERS:\n"
                "       p[p..]   The argument is a pointer to [an array of / a struct of] specified type\n"
                "                The number of p's indicates the pointer depth\n"
                "   ARRAYS:\n"
                "       Arrays can be used for both arguments and return values.\n"
                "       In arguments, the size can be optionally inferred from the values.\n"
                "       In return values, the size must be specified\n"
                "            Values can be specified as unspaced comma delimitted or as a single unbroken hex value\n"
                "       a<type> <v>,<v>,..   The argument is an array of the specified type and inferred size (only valid for arguments)\n"
                "       a<type> 0xdeadbe..   The argument is an array of the specified type and inferred size (only valid for arguments)\n"
                "            Sizes are given explicitly following the type character flag and can be static or dynamic\n"
                "       a<type><size>        The array is of the following static <size>\n"
                "       a<type>t<argnum>     (Notice the `t` flag!) The size of the array is dependent on the value of <argnum>\n"
                "                            <argnum> is 0 for return value or n for the nth (1-indexed) argument \n"
                "       In arguments, where the size is specified, the value can be given as NULL, if the function is expected to allocate the array\n"
                "   ARRAY EXAMPLES:\n");
        printf( "     * For a function: int return_buffer(char** outbuff) which returns size\n");
        printf( "     %s libexample.so v return_buffer -past2 NULL -pi 0\n", argv[0]);
        printf( "     * Or alternatively if it were: void return_buffer(char** outbuff, size_t* outsize)\n");
        printf( "     %s libexample.so i return_buffer -past0 NULL\n", argv[0]);
        printf( "     * For a function: int add_all_ints(int** nums, size_t size) which returns sum\n");
        printf( "     %s libexample.so i add_all_ints -ai 1,2,3,4,5 -i 5\n", argv[0]);
        printf( "\n");
        printf( "   STRUCTS:\n"
                "      Structs can be used for both arguments and return values\n"
                "      The general syntax is [-]S: <arg> [<arg>...] :S \n"
                "      For arguments the dash is included and values are given (with optional typeflags)\n"
                "      -S: [-typeflag] <arg> [[-typeflag] <arg>...] S: \n"
                "      For return values the dash is omitted and only dashless typeflags are given\n"
                "      S: <typeflag> [<typeflag>...] :S\n"
                "      As always, the S: can be prefixed with p's to indicate struct pointers of arbitrary depth\n"
                "      Nested structs are permitted but be careful with the open and close tags\n"
                "    STRUCT EXAMPLES:\n"
                "      Given a struct: struct mystruct { int x; char* s; }\n"
                "      * For a function: void print_struct(struct mystruct s)\n"
                "      %s libexample.so v print_struct -S: 3 \"hello world\" :S\n", argv[0]);
        printf( "      * For a function: struct mystruct return_struct(int x, char* s)\n"
                "      %s libexample.so S: i s :S 5 \"hello world\"\n", argv[0]);
        printf( "      * For a function: modifyStruct(struct mystruct* s)\n"
                "      %s libexample.so v modifyStruct -pS: 3 \"hello world\" :S\n", argv[0]);
        return 0;
    }
    else if (argc < 4) {
        fprintf(stderr, "%s %s\nUsage: %s [--help] <library> <return_typeflag> <function_name> [[-typeflag] <arg>...]\n", NAME,VERSION,argv[0]);
        return 1;
    }



    //print all args
    #ifdef DEBUG
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }
    setbuf(stdout, NULL); // disable buffering for stdout
    setbuf(stderr, NULL); // disable buffering for stderr
    #endif

    // Step 1: Resolve the library path
    // For now we've delegated that call to parse_arguments


    // Step 2: Parse command-line arguments
    FunctionCallInfo* call_info = parse_arguments(argc, argv);
    // convert_all_arrays_to_arginfo_ptr_sized_after_parsing(call_info); <-- handled inside parse_arguments now
    

    // Step 2.5 (optional): Print the parsed function call call_info
    log_function_call_info(call_info);

    // Step 3: Invoke the specified function

    void* lib_handle = dlopen(call_info->library_path, RTLD_LAZY);
    if (!lib_handle) {
        fprintf(stderr, "Failed to load library: %s\n", dlerror());
        return -1;
    }

    void (*func)(void);
    *(void**)(&func) = dlsym(lib_handle, call_info->function_name);

    int invoke_result = invoke_dynamic_function(call_info,func);
    if (invoke_result != 0) {
        fprintf(stderr, "Error: Function invocation failed\n");
    } else {

        // Step 4: Print the return value and any modified arguments

        printf("Function returned: ");

        // format_and_print_arg_type(&call_info->return_var);
        format_and_print_arg_value(&call_info->info.return_var);
        printf("\n");

        for (int i = 0; i < call_info->info.arg_count; i++) {
            // if it could have been modified, print it
            // TODO keep track of the original value and compare
            if (call_info->info.args[i].is_array || call_info->info.args[i].pointer_depth > 0) {
                printf("Arg %d after function return: ", i);
                format_and_print_arg_type(&call_info->info.args[i]);
                printf(" ");
                format_and_print_arg_value(&call_info->info.args[i]);
                printf("\n");
            }
        }
    }

    // Clean up

    // free() introduces problems with functions returning literals that cannot be freed, which cannot be distinguished from heap-allocated memory
    // and we are anyway exiting the program, so we don't actually need to free memory
    // freeFunctionCallInfo(call_info); 

    // Wait to close the library until after we're done with everything in case it returns pointers to literals stored in the library
    dlclose(lib_handle);

    return invoke_result;
}
