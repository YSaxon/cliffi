#include "argparser.h"
#include "invoke_handler.h"
#include "library_manager.h"
#include "parse_address.h"
#include "return_formatter.h"
#include "types_and_utils.h"
#include "system_constants.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "exception_handling.h"
#include "repl.h"
#include "server.h"

#if  !defined(_WIN32) && !defined(_WIN64)
#include <readline/history.h>
#include <readline/readline.h>
#include <sys/wait.h> // same as above
#include <unistd.h>   // only used for forking for --repltest repl test harness mode
#endif

#include "subprocess.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "shims.h"


const char* NAME = "cliffi";
const char* VERSION = "v1.12.6";
const char* BASIC_USAGE_STRING = "<library> <return_typeflag> <function_name> [[-typeflag] <arg>.. [ ... <varargs>..] ]\n";


void print_usage(char* argv0) {
    fprintf(stdout, "%s %s\n", NAME, VERSION);
    fprintf(stdout, "Usage: %s %s\n", argv0, BASIC_USAGE_STRING);
    fprintf(stdout, "  [--help]         Print this help message\n"
           "  [--repl]         Start the REPL\n"
           "  [--server] [--host <host>] [--port <port>]      Start a REPL bind server on host (default %s) and port (default %s)\n",
           DEFAULT_SERVER_HOST, DEFAULT_SERVER_PORT);
    fprintf(stdout,
           "  <library>        The path to the shared library containing the function to invoke\n"
           "                   or the name of the library if it is in the system path\n"
           "  <typeflag>       The type of the return value of the function to invoke\n"
           "                   v for void, i for int, s for string, etc\n"
           "  <function_name>  The name of the function to invoke\n"
           "  [-<typeflag>] <arg> The argument values to pass to the function\n"
           "                   Types will be inferred if not prefixed with flags\n"
           "                   Flags look like -i for int, -s for string, etc\n"
           "  ...              Mark the position of varargs in the function signature if applicable\n");
    fprintf(stdout, "\n"
           "  BASIC EXAMPLES:\n");
    fprintf(stdout, "         %s libexample.so i addints 3 4\n", argv0);
    fprintf(stdout, "         %s path/to/libexample.so v dofoo\n", argv0);
    fprintf(stdout, "         %s ./libexample.so s concatstrings -s hello -s world\n", argv0);
    fprintf(stdout, "         %s libexample.so s concatstrings hello world\n", argv0);
    fprintf(stdout, "         %s libexample.so d multdoubles -d 1.5 1.5d\n", argv0);
    fprintf(stdout, "         %s %s%s i printf 'Here is a number: %%.3f' ... 4.5", argv0, DEFAULT_LIBC_NAME, DEFAULT_LIBRARY_EXTENSION);
    fprintf(stdout, "\n");
    fprintf(stdout, "  TYPES:\n"
           "     The primitive typeflags are:\n");
    fprintf(stdout, "       %c for void, only allowed as a return type, and does not accept prefixes\n", TYPE_VOID);
    fprintf(stdout, "       %c for char\n", TYPE_CHAR);
    fprintf(stdout, "       %c for short\n", TYPE_SHORT);
    fprintf(stdout, "       %c for int\n", TYPE_INT);
    fprintf(stdout, "       %c for long\n", TYPE_LONG);
    fprintf(stdout, "       %c for unsigned char\n", TYPE_UCHAR);
    fprintf(stdout, "       %c for unsigned short\n", TYPE_USHORT);
    fprintf(stdout, "       %c for unsigned int\n", TYPE_UINT);
    fprintf(stdout, "       %c for unsigned long\n", TYPE_ULONG);
    fprintf(stdout, "       %c for float, can also be specified by suffixing the value with f\n", TYPE_FLOAT);
    fprintf(stdout, "       %c for double, can also be specified by suffixing the value with d\n", TYPE_DOUBLE);
    fprintf(stdout, "       %c for cstring (ie null terminated char*)\n", TYPE_STRING);
    fprintf(stdout, "       %c for arbitrary pointer (ie void*) specified by address\n", TYPE_VOIDPOINTER);
    fprintf(stdout, "\n");
    fprintf(stdout, "  POINTERS AND ARRAYS AND STRUCTS:\n"
           "     Typeflags can include additional flag prefixes to specify pointers, arrays or structs:\n"
           "     <typeflag> = [p[p..]]<primitive_type>\n"
           "     <typeflag> = [p[p..]][a<size>|t<argnum>][p[p..]]<primitive_type>\n"
           "     <typeflag> = [p[p..]]S: <arg> <arg>.. :S \n"
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
           "       Note that pa<type> means a pointer to an array of type, while ap<type> means an array of <type> pointers \n"
           "   ARRAY EXAMPLES:\n");
    fprintf(stdout, "     * For a function: int return_buffer(char** outbuff) which returns size\n");
    fprintf(stdout, "     %s libexample.so v return_buffer -past2 NULL -pi 0\n", argv0);
    fprintf(stdout, "     * Or alternatively if it were: void return_buffer(char** outbuff, size_t* outsize)\n");
    fprintf(stdout, "     %s libexample.so i return_buffer -past0 NULL\n", argv0);
    fprintf(stdout, "     * For a function: int add_all_ints(int** nums, size_t size) which returns sum\n");
    fprintf(stdout, "     %s libexample.so i add_all_ints -ai 1,2,3,4,5 -i 5\n", argv0);
    fprintf(stdout, "\n");
    fprintf(stdout, "   STRUCTS:\n"
           "      Structs can be used for both arguments and return values\n"
           "      The general syntax is [-]S[K]: <arg> [<arg>..] :S \n"
           "      For arguments the dash is included and values are given (with optional typeflags)\n"
           "      -S[K]: [-typeflag] <arg> [[-typeflag] <arg>..] S: \n"
           "      For return values the dash is omitted and only dashless typeflags are given\n"
           "      S[K]: <typeflag> [<typeflag>..] :S\n"
           "      The optional K following the S denotes a pacKed struct, ie __attribute__((packed)), where the fields are unpadded\n"
           "      As always, the S: can be prefixed with p's to indicate struct pointers of arbitrary depth\n"
           "      Nested structs are permitted but be careful with the open and close tags\n"
           "    STRUCT EXAMPLES:\n"
           "      Given a struct: struct mystruct { int x; char* s; }\n"
           "      * For a function: void print_struct(struct mystruct s)\n");
    fprintf(stdout, "      %s libexample.so v print_struct -S: 3 \"hello world\" :S\n", argv0);
    fprintf(stdout, "      * For a function: struct mystruct return_struct(int x, char* s)\n");
    fprintf(stdout, "      %s libexample.so S: i s :S 5 \"hello world\"\n", argv0);
    fprintf(stdout, "      * For a function: modifyStruct(struct mystruct* s)\n");
    fprintf(stdout, "      %s libexample.so v modifyStruct -pS: 3 \"hello world\" :S\n", argv0);
    fprintf(stdout, "\n");
    fprintf(stdout, "  VARARGS:\n"
           "     If a function takes varargs the position of the varargs should be specified with the `...` flag\n"
           "     The `...` flag may sometimes be the first arg if the function takes all varargs, or the last for a function that takes varargs where none are being passed\n"
           "     The varargs themselves are the same as any other function args and can be with or without typeflags\n"
           "     (Floats and types shorter than int will be upgraded for you automatically)\n"
           "    VARARGS EXAMPLES:\n");
    fprintf(stdout, "      %s %s%s i printf 'Hello %%s, your number is: %%.3f' ... bob 4.5\n", argv0, DEFAULT_LIBC_NAME, DEFAULT_LIBRARY_EXTENSION);
    fprintf(stdout, "      %s %s%s i printf 'This is just a static string' ... \n", argv0, DEFAULT_LIBC_NAME, DEFAULT_LIBRARY_EXTENSION);
    fprintf(stdout, "      %s some_lib.so v func_taking_all_varargs ... -i 3 -s hello\n", argv0);
}

void print_function_return(FunctionCallInfo* call_info){
     setCodeSectionForSegfaultHandler("invoke_and_print_return_value : while printing values");

        // Step 4: Print the return value and any modified arguments

        fprintf(stdout, "Function returned: ");

        // format_and_print_arg_type(call_info->return_var);
        format_and_print_arg_value(call_info->info.return_var);
        fprintf(stdout, "\n");

        for (int i = 0; i < call_info->info.arg_count; i++) {
            // if it could have been modified, print it
            // TODO keep track of the original value and compare
            if (call_info->info.args[i]->is_array || call_info->info.args[i]->pointer_depth > 0) {
                fprintf(stdout, "Arg %d after function return: ", i);
                format_and_print_arg_type(call_info->info.args[i]);
                fprintf(stdout, " ");
                format_and_print_arg_value(call_info->info.args[i]);
                fprintf(stdout, "\n");
            }
        }
    unsetCodeSectionForSegfaultHandler();
    }


int invoke_and_print_return_value(FunctionCallInfo* call_info, void (*func)(void)) {
    int invoke_result = invoke_dynamic_function(call_info, func);
    if (invoke_result != 0) {
        fprintf(stderr, "Error: Function invocation failed\n");
    } else {
        print_function_return(call_info);
    }
    return invoke_result;
}

void* loadFunctionHandle(void* lib_handle, const char* function_name) {

    if (isHexFormat(function_name)) { // parse it as an offset of the library
        void* address_offset_relative_to_lib = getAddressFromStoredOffsetRelativeToLibLoadedAtAddress(lib_handle, function_name);
        if (address_offset_relative_to_lib != NULL) {
            fprintf(stdout, "Parsed func '%s' as relative address to the library offset, 0x%" PRIxPTR "\n", function_name, (uintptr_t)address_offset_relative_to_lib);
            return address_offset_relative_to_lib;
        } else {
            raiseException(1,  "Error: Could not find a stored offset for your library. Try again after running calculate_offset\n");
            return NULL;
        }
    }

    void* addressDirectly = tryGetAddressFromAddressStringOrNameOfCoercableVariable(function_name);
    if (addressDirectly != NULL) {
        fprintf(stdout, "Parsed func '%s' as an absolute address -> 0x%" PRIxPTR "\n", function_name, (uintptr_t)addressDirectly);
        return addressDirectly;
    }


    void (*func)(void) = NULL;
#ifdef _WIN32
    FARPROC temp = GetProcAddress(lib_handle, function_name);
    if (temp != NULL) {
        memcpy(&func, &temp, sizeof(temp)); // to fix warning re dereferencing type-punned pointer
    }
#else
    *(void**)(&func) = dlsym(lib_handle, function_name);
#endif
    if (!func) {
#ifdef _WIN32
        raiseException(1,  "Failed to find function: %lu\n", GetLastError());

#else
        raiseException(1,  "Failed to find function: %s\n", dlerror());
#endif
        return NULL; // just to silence a warning, not actually reachable
    }
    return func;
}



bool checkAndRunCliffiInitWithPath(char* path) {
    // look for a file .cliffi_init in the specified path and run it if it exists
    char* cliffi_init_path = ".cliffi_init";
    char* full_path = malloc(strlen(path) + strlen(cliffi_init_path) + 2);
    strcpy(full_path, path);
    strcat(full_path, "/");
    strcat(full_path, cliffi_init_path);
    FILE* file = fopen(full_path, "r");
    if (file == NULL) {
        free(full_path);
        return false;
    } else {
        fprintf(stdout, "Running cliffi init file at %s\n", full_path);
        char* line = NULL;
        size_t len = 0;
        ssize_t read;
        while ((read = getline(&line, &len, file)) != -1) {
            if (line[read - 1] == '\n') {
                line[read - 1] = '\0';
            }
            TRY
                int breakRepl = parseREPLCommand(line);
                if (breakRepl) break; //TODO: why is this needed?
            CATCHALL
                printException();
                fprintf(stderr, "Error encountered in processing a line from .cliffi_init file: %s\n", line);
            END_TRY
        }
        free(line);
        fclose(file);
    }
    free(full_path);
    return true;
}

void checkAndRunCliffiInits() {
    // look for a file .cliffi_init in the current directory and run it if it exists
    // otherwise look for a file .cliffi_init in the home directory and run it if it exists
    if (!checkAndRunCliffiInitWithPath(".")) {
        char* home = getenv("HOME");
        if (home != NULL) {
            checkAndRunCliffiInitWithPath(home);
        }
    }
}

char* g_executable_path = NULL;
#ifndef CLIFFI_UNIT_TESTING // don't defined main() when compiling for unit tests to avoid a collision
int main(int argc, char* argv[]) {
    setbuf(stdout, NULL); // disable buffering for stdout
    setbuf(stderr, NULL); // disable buffering for stderr
    main_method_install_exception_handlers();
    g_executable_path = argv[0]; // Set the global path

    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "--repltest") == 0) {
        if (argc > 2 && strcmp(argv[2], "--noexitonfail") == 0) {
            argc--;
            argv++;
            isTestEnvExit1OnFail = false;
        } else {
            isTestEnvExit1OnFail = true;
        }

        // Define the command to launch ourselves in REPL mode.
        const char* command_line[4];
        command_line[0] = g_executable_path;
        command_line[1] = "--repl";
        if (isTestEnvExit1OnFail) {
            command_line[2] = "--exit-on-fail"; // Pass the state explicitly
            command_line[3] = NULL;
        } else {
            command_line[2] = NULL;
        }

        struct subprocess_s subprocess;

        // Create the subprocess. Inherit the parent's environment so it can find libraries.
        int options = subprocess_option_inherit_environment | subprocess_option_enable_async | subprocess_option_no_window ;
        int result = subprocess_create(command_line, options, &subprocess);
        if (result != 0) {
            fprintf(stderr, "Error: --repltest failed to create subprocess.\n");
            return 1;
        }

        // Get a FILE pointer to the child's standard input.
        FILE* p_stdin = subprocess_stdin(&subprocess);
        if (p_stdin == NULL) {
            fprintf(stderr, "Error: --repltest couldn't get subprocess stdin.\n");
            return 1;
        }

        // Write all test arguments as a single line to the child process.
        for (int i = 2; i < argc; i++) {
            fputs(argv[i], p_stdin);
            fputs(" ", p_stdin);
        }
        fputs("\n", p_stdin);

        // VERY IMPORTANT: Close the child's stdin pipe. This sends an EOF,
        // allowing the child's `fgets`/`getline` loop to terminate naturally.
        fclose(p_stdin);

        char buffer[1024];
        unsigned int bytes_read;
        while (subprocess_alive(&subprocess)) {
            bytes_read = subprocess_read_stdout(&subprocess, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                fprintf(stdout, "%s", buffer);
            }

            bytes_read = subprocess_read_stderr(&subprocess, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                fprintf(stderr, "%s", buffer);
            }
        }

        // Wait for the child process to complete and get its exit code.
        int return_code;
        result = subprocess_join(&subprocess, &return_code);
        if (result != 0) {
            fprintf(stderr, "Error: --repltest failed to join subprocess.\n");
        }

        // Clean up subprocess resources.
        subprocess_destroy(&subprocess);

        // Exit with the same code as the child process, just like the original test.
        if (isTestEnvExit1OnFail && return_code != 0) {
            fprintf(stderr, "\n\nTest failed with exit code %d\n", return_code);
        } else {
            fprintf(stdout, "\n\nTest completed with exit code %d\n", return_code);
        }
        exit(return_code);
    } else if (argc > 1 && strcmp(argv[1], "--repl") == 0)
    replmode: {
        isTestEnvExit1OnFail = false; // Default to exiting with 1 on failure in REPL mode
           for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--exit-on-fail") == 0) {
            isTestEnvExit1OnFail = true;
            argc--;
            argv++;
            break;
        }
    }

        checkAndRunCliffiInits();
        // rl_completion_entry_function = (Function*)cliffi_completion;
        rl_bind_key('\t', rl_complete);
        using_history();
        read_history(".cliffi_history");
        fprintf(stderr, "cliffi %s. Starting REPL... Type 'help' for assistance. Type 'exit' to quit:\n", VERSION);
        fflush(stderr);

        // Start the REPL
        startRepl();

        return 0;
    } else if (argc > 1 && strcmp(argv[1], "--server") == 0)
    {
        const char* server_host = DEFAULT_SERVER_HOST;
        const char* server_port_str = DEFAULT_SERVER_PORT;
        bool server_fork_mode = false; // Default to single client mode
        for (int i = 2; i < argc; ++i) {
            if ((strcmp(argv[i], "--host") == 0 || strcmp(argv[i], "-h") == 0 )&& i + 1 < argc) {
                server_host = argv[++i];
            } else if ((strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) && i + 1 < argc) {
                server_port_str = argv[++i];
            } else if (strcmp(argv[i], "--help") == 0) {
                // print_usage(argv[0]); // Your existing usage
                printf("Server options:\n");
                printf("  --server            Run in server mode.\n");
                printf("  -h | --host <address>    Host address to bind to (default: %s).\n", DEFAULT_SERVER_HOST);
                printf("  -p | --port <port>       Port to listen on (default: %s).\n", DEFAULT_SERVER_PORT);
                return 0;
            }
        }
    int server_result = start_cliffi_tcp_server(server_host, server_port_str);
    return server_result;
    } else if (argc < 4) {
#define DASHDASHREPL " [--repl]"
            fprintf(stderr, "%s %s\nUsage: %s [--help]%s %s\n", NAME, VERSION, argv[0], DASHDASHREPL, BASIC_USAGE_STRING);
            return 1;
        }

// print all args
#ifdef DEBUG
    for (int i = 0; i < argc; i++) {
        fprintf(stdout, "argv[%d] = %s\n", i, argv[i]);
    }
#endif

    TRY
    // run .cliffi_init file if exists
    checkAndRunCliffiInits();

    CATCHALL
        fprintf(stderr, "Error encountered in processing .cliffi_init file. Ignoring and proceeding to command execution.\n");
        printException();
    END_TRY

    // Step 1: Resolve the library path
    // For now we've delegated that call to parse_arguments

    TRY
    // Step 2: Parse command-line arguments
    FunctionCallInfo* call_info = parse_arguments(argc - 1, argv + 1); // skip the program name
    // convert_all_arrays_to_arginfo_ptr_sized_after_parsing(call_info); <-- handled inside parse_arguments now

    // Step 2.5 (optional): Print the parsed function call call_info
    log_function_call_info(call_info);

    // Step 3: Invoke the specified function

    void* lib_handle = getOrLoadLibrary(call_info->library_path);

    void* func = loadFunctionHandle(lib_handle, call_info->function_name);

    int invoke_result = invoke_and_print_return_value(call_info, func);

    // Clean up

    // free() introduces problems with functions returning literals that cannot be freed, which cannot be distinguished from heap-allocated memory
    // and we are anyway exiting the program, so we don't actually need to free memory
    // freeFunctionCallInfo(call_info);

    // Wait to close the library until after we're done with everything in case it returns pointers to literals stored in the library
#ifdef _WIN32
    FreeLibrary(lib_handle);
#else
    dlclose(lib_handle);
    #endif

    return invoke_result;

    CATCHALL
        printException();
        exit(1);
    END_TRY
}
#endif
