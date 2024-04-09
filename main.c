#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "library_path_resolver.h"
#include "argparser.h"
#include "invoke_handler.h"
#include "return_formatter.h"
#include "types_and_utils.h"
#include <setjmp.h>
#include <signal.h>
#include "library_manager.h"
#include "var_map.h"


#if !defined(_WIN32) && !defined(_WIN64)
#define use_readline
#endif

#if defined(_WIN32) || defined(_WIN64)
#define sigjmp_buf jmp_buf
#define sigsetjmp(env, save) setjmp(env)
#define siglongjmp(env, val) longjmp(env, val)
#endif

#ifdef use_readline
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h> // only used for forking for --repltest repl test harness mode
#include <sys/wait.h> // same as above
#if !defined(__ANDROID__)
#include <wordexp.h> //only used for tokenizing in the repl
#else
#include "wordexp.h" // cmakelists will download this for android to the build dir
#endif
#endif

#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))) && !defined(__ANDROID__)
#define use_backtrace
#endif

#ifdef use_backtrace
#include <execinfo.h>
#endif

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

const char* NAME = "cliffi";
const char* VERSION = "v1.1.6";
const char* BASIC_USAGE_STRING = "<library> <return_typeflag> <function_name> [[-typeflag] <arg>.. [ ... <varargs>..] ]\n";

sigjmp_buf jmpBuffer;

#ifdef use_backtrace
void printStackTrace() {
    void *array[10];
    size_t size;
    char **strings;
    size_t i;

    size = backtrace(array, 10);

    if (size == 0) {
        // fprintf(stderr, "No stack trace available\n");
        return;
    }

    strings = backtrace_symbols(array, size);

    fprintf(stderr, "Stack trace:\n");
    for (i = 0; i < size; i++) {
        fprintf(stderr, "%s\n", strings[i]);
    }

    free(strings);
}
#endif

void exit_or_restart(int status) {
    #ifdef use_backtrace
    if (status != 0) printStackTrace();
    #endif
    siglongjmp(jmpBuffer, status);
}

void logSegfault() {
    fprintf(stderr, "Segmentation fault occurred\n");
}

void handleSegfault(int signal) {
    // Log the segfault
    logSegfault();

    // Perform cleanup if needed

    // Jump back to the REPL loop
    siglongjmp(jmpBuffer, 1);
}


void print_usage(char* argv0){
        printf( "%s %s\n", NAME, VERSION);
        printf( "Usage: %s %s\n", argv0, BASIC_USAGE_STRING);
        printf( "  [--help]         Print this help message\n"
        #ifdef use_readline
                "  [--repl]         Start the REPL\n"
        #endif
                "  <library>        The path to the shared library containing the function to invoke\n"
                "                   or the name of the library if it is in the system path\n"
                "  <typeflag>       The type of the return value of the function to invoke\n"
                "                   v for void, i for int, s for string, etc\n"
                "  <function_name>  The name of the function to invoke\n"
                "  [-<typeflag>] <arg> The argument values to pass to the function\n"
                "                   Types will be inferred if not prefixed with flags\n"
                "                   Flags look like -i for int, -s for string, etc\n"
                "  ...              Mark the position of varargs in the function signature if applicable\n");
        printf( "\n"
                "  BASIC EXAMPLES:\n"
                "         %s libexample.so i addints 3 4\n", argv0);
        printf( "         %s path/to/libexample.so v dofoo\n", argv0);
        printf( "         %s ./libexample.so s concatstrings -s hello -s world\n", argv0);
        printf( "         %s libexample.so s concatstrings hello world\n", argv0);
        printf( "         %s libexample.so d multdoubles -d 1.5 1.5d\n", argv0);
        printf( "         %s libc.so i printf 'Here is a number: %%.3f' ... 4.5", argv0);
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
        printf( "       %c for arbitrary pointer (ie void*) specified by address\n", TYPE_VOIDPOINTER);
        printf("\n");
        printf( "  POINTERS AND ARRAYS AND STRUCTS:\n"
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
        printf( "     * For a function: int return_buffer(char** outbuff) which returns size\n");
        printf( "     %s libexample.so v return_buffer -past2 NULL -pi 0\n", argv0);
        printf( "     * Or alternatively if it were: void return_buffer(char** outbuff, size_t* outsize)\n");
        printf( "     %s libexample.so i return_buffer -past0 NULL\n", argv0);
        printf( "     * For a function: int add_all_ints(int** nums, size_t size) which returns sum\n");
        printf( "     %s libexample.so i add_all_ints -ai 1,2,3,4,5 -i 5\n", argv0);
        printf( "\n");
        printf( "   STRUCTS:\n"
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
                "      * For a function: void print_struct(struct mystruct s)\n"
                "      %s libexample.so v print_struct -S: 3 \"hello world\" :S\n", argv0);
        printf( "      * For a function: struct mystruct return_struct(int x, char* s)\n"
                "      %s libexample.so S: i s :S 5 \"hello world\"\n", argv0);
        printf( "      * For a function: modifyStruct(struct mystruct* s)\n"
                "      %s libexample.so v modifyStruct -pS: 3 \"hello world\" :S\n", argv0);
        printf( "\n");
        printf( "  VARARGS:\n"
                "     If a function takes varargs the position of the varargs should be specified with the `...` flag\n"
                "     The `...` flag may sometimes be the first arg if the function takes all varargs, or the last for a function that takes varargs where none are being passed\n"
                "     The varargs themselves are the same as any other function args and can be with or without typeflags\n"
                "     (Floats and types shorter than int will be upgraded for you automatically)\n"
                "    VARARGS EXAMPLES:\n"
                "      %s libc.so i printf 'Hello %%s, your number is: %%.3f' ... bob 4.5\n", argv0);
        printf( "      %s libc.so i printf 'This is just a static string' ... \n", argv0);
        printf( "      %s some_lib.so v func_taking_all_varargs ... -i 3 -s hello\n", argv0);
}


int invoke_and_print_return_value(FunctionCallInfo* call_info, void (*func)(void)) {
    int invoke_result = invoke_dynamic_function(call_info,func);
    if (invoke_result != 0) {
        fprintf(stderr, "Error: Function invocation failed\n");
    } else {

        // Step 4: Print the return value and any modified arguments

        printf("Function returned: ");

        // format_and_print_arg_type(call_info->return_var);
        format_and_print_arg_value(call_info->info.return_var);
        printf("\n");

        for (int i = 0; i < call_info->info.arg_count; i++) {
            // if it could have been modified, print it
            // TODO keep track of the original value and compare
            if (call_info->info.args[i]->is_array || call_info->info.args[i]->pointer_depth > 0) {
                printf("Arg %d after function return: ", i);
                format_and_print_arg_type(call_info->info.args[i]);
                printf(" ");
                format_and_print_arg_value(call_info->info.args[i]);
                printf("\n");
            }
        }
    }
    return invoke_result;
}

void* loadFunctionHandle(void* lib_handle, const char* function_name) {
    void (*func)(void);
    #ifdef _WIN32
    FARPROC temp = GetProcAddress(lib_handle, function_name);
    if (temp != NULL) {
        memcpy(&func, &temp, sizeof(temp)); // to fix warning re dereferencing type-punned pointer
    } else {
        void (*func)(void) = NULL; // Initialize func to NULL
    }

    #else
    *(void**)(&func) = dlsym(lib_handle, function_name);
    #endif
    if (!func) {
        #ifdef _WIN32
        fprintf(stderr, "Failed to find function: %lu\n", GetLastError());
        #else
        fprintf(stderr, "Failed to find function: %s\n", dlerror());
        #endif
        exit_or_restart(1);
    }
    return func;
}

#ifdef use_readline

int tokenize(const char* str, int* argc, char*** argv) {
    wordexp_t p;
    int result = wordexp(str, &p, 0);
    *argc = p.we_wordc;
    *argv = p.we_wordv;
    return result;
}

void printVariableWithArgInfo(char* varName, ArgInfo* arg) {
        format_and_print_arg_type(arg);
        printf(" %s = ", varName);
        format_and_print_arg_value(arg);
        printf("\n");
}

void parsePrintVariable(char* varName) {
    ArgInfo* arg = getVar(varName);
    if (arg == NULL) {
        fprintf(stderr, "Error printing var: Variable %s not found.\n", varName);
    } else {
        printVariableWithArgInfo(varName, arg);
    }
}

void parseSetVariableWithNameAndValue(char* varName, int varValueCount, char** varValues) {

        if (varName==NULL || strlen(varName) == 0) {
            fprintf(stderr, "Variable name cannot be empty.\n");
            return;
        } else if (varValues== NULL || varValueCount == 0 || strlen(varValues[0]) == 0) {
            fprintf(stderr, "Variable value cannot be empty.\n");
            return;
        }

        if (strlen(varName) == 1 && charToType(*varName) != TYPE_UNKNOWN) {
            fprintf(stderr, "Variable name cannot be a character used in parsing types, such as %s which is used for %s\n", varName, typeToString(charToType(*varName)));
            return;
        } else if (*varName == '-') {
            fprintf(stderr, "Variable names cannot start with a dash.\n");
            return;
        } else if (isAllDigits(varName) || isHexFormat(varName) || isFloatingPoint(varName)) {
            fprintf(stderr, "Variable names cannot be a number.\n");
            return;
        }

        int args_used = 0;
        ArgInfo* arg = parse_one_arg(varValueCount, varValues, &args_used, false);
        if (args_used+1 != varValueCount) {
            fprintf(stderr, "Invalid variable value.\n");
            //maybe free the arg?
            return;
        }
        printVariableWithArgInfo(varName, arg);
        setVar(varName, arg);
        //Should we check if the variable already existed and free the previous value? Or maybe keep a reference count?
}

void executeREPLCommand(char* command){
    int argc;
    char ** argv;
    if (tokenize(command, &argc, &argv)!=0) {
        fprintf(stderr, "Error: Tokenization failed for command\n");
        return;
    }

    // syntactic sugar for set <var> <value> and print <var>
    if (argc == 1) {
        parsePrintVariable(argv[0]);
        return;
    } else if (argc >= 3 && strcmp(argv[1], "=") == 0) {
        parseSetVariableWithNameAndValue(argv[0], argc-2, argv+2);
        return;
    }


    if (argc < 3) {
        fprintf(stderr, "Invalid command '%s'. Type 'help' for assistance.\n", command);
        return;
    }
    FunctionCallInfo* call_info = parse_arguments(argc, argv);
    log_function_call_info(call_info);
    void* lib_handle = getOrLoadLibrary(call_info->library_path);
    if (lib_handle == NULL) {
        fprintf(stderr, "Failed to load library: %s\n", call_info->library_path);
        return;
    }
    void* func = loadFunctionHandle(lib_handle, call_info->function_name);

    int invoke_result = invoke_and_print_return_value(call_info, func);
    if (invoke_result != 0) {
        fprintf(stderr, "Error: Function invocation failed\n");
    }
}

char** cliffi_completion(const char* text, int state) {
    fprintf(stderr, "Not implemented");
    return NULL;
    // if (!text || text[0] == '\0') {
    //     return (char*[]){"test","complete",NULL};
    // }
    // else {
    //     return (char*[]){"not","blank",NULL};
    // }
    // calculate what kind of token we are at
    // if we are at the first token, we are looking for a library and we should delegate to rl_filename_completion_function (and also a list of opened libraries from the library manager)
    // if we are at the second token, we are looking for a return type and we should complete typeflags
    // if we are at the third token, we are looking for a function name and we should complete function names (I guess we could use dlsym to get the list of functions in the library)
    // from there we would really need to apply the parser to see if we are in a typeflag or an argument etc, and go from there
    }

void parseSetVariable(char* varCommand) {

        int argc;
        char ** argv;
        if (tokenize(varCommand, &argc, &argv)!=0) {
            fprintf(stderr, "Error: Tokenization failed for variable value\n");
            return;
        }

        parseSetVariableWithNameAndValue(argv[0], argc-1, argv+1);
}

void startRepl() {

    char* command;

    while ((command = readline("> ")) != NULL) {
        command = trim_whitespace(command);
        if (strlen(command) > 0) {
            // fprintf(stderr, "Command: %s\n", command);
            HIST_ENTRY * last_command = history_get(history_length);
            if (last_command == NULL || strcmp(command, last_command->line) != 0)
            {
                add_history(command);
                write_history(".cliffi_history");
            }
            if (strcmp(command, "quit") == 0 || strcmp(command, "exit") == 0) {
                closeAllLibraries();
                break;
            } else if (strcmp(command, "help") == 0) {
                printf( "Running a command:\n"
                        "  %s\n", BASIC_USAGE_STRING);
                printf( "Documentation:\n"
                        "  help: Print this help message\n"
                        "  docs: Print the cliffi docs\n"
                        "Variables:\n"
                        "  set <var> <value>: Set a variable\n"
                        "  print <var>: Print the value of a variable\n"
                        "Shared Library Management:\n"
                        "  list: List all opened libraries\n"
                        "  close <library>: Close the specified library\n"
                        "  closeall: Close all opened libraries\n"
                        "REPL Management:\n"
                        "  exit: Quit the REPL\n");
            } else if (strcmp(command, "docs") == 0) {
                print_usage(">");
            } else if (strcmp(command, "list") == 0) {
                listOpenedLibraries();
            } else if (strncmp(command, "close", 5) == 0) {
                char* libraryName = command + 5;
                while (*libraryName == ' ') {
                    libraryName++;
                }
                closeLibrary(libraryName);
            } else if (strcmp(command, "closeall") == 0) {
                closeAllLibraries();
            } else if (strncmp(command, "set ", 4) == 0) {
                char* set_var_command = command + 4;
                parseSetVariable(set_var_command);
            } else if (strncmp(command, "print ", 6) == 0) {
                parsePrintVariable(command + 6);
            } else {
                executeREPLCommand(command); // also handles syntactic sugar for set and print
            }
        }
        free(command);
    }
}

#endif 






int main(int argc, char* argv[]) {

    setbuf(stdout, NULL); // disable buffering for stdout
    setbuf(stderr, NULL); // disable buffering for stderr
    signal(SIGSEGV, handleSegfault);
    if (sigsetjmp(jmpBuffer, 1) != 0) {
        fprintf(stderr, "Error occurred. Exiting...\n");
        return 1;
    }
    
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    #ifdef use_readline
    if (argc > 1 && strcmp(argv[1], "--repltest") == 0){
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe");
            return 1;
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            return 1;
        }

        if (pid == 0) { // Child process
            close(pipefd[1]); // Close write end of pipe
            dup2(pipefd[0], STDIN_FILENO); // Redirect STDIN to read from pipe
            close(pipefd[0]); // Close read end, not needed anymore
            goto replmode;
        } else { // Parent process
            close(pipefd[0]); // Close the write end of the pipe
            for (int i = 2; i < argc; i++) {
                write(pipefd[1], argv[i], strlen(argv[i]));
                write(pipefd[1], " ", 1);
            }
            close(pipefd[1]); // Close the write end of the pipe
            waitpid(pid, NULL, 0);
            return 0;
        }
    } 
    else if (argc > 1 && strcmp(argv[1], "--repl") == 0) replmode: {
        initializeLibraryManager();
        // rl_completion_entry_function = (Function*)cliffi_completion;
        rl_bind_key('\t', rl_complete);
        using_history();
        read_history(".cliffi_history");
        fprintf(stderr, "cliffi %s. Starting REPL... Type 'help' for assistance. Type 'exit' to quit:\n", VERSION);

        // Start the REPL
        if (sigsetjmp(jmpBuffer, 1) == 0) {
            startRepl();
        } else {
            fprintf(stderr, "Error occurred. Restarting REPL...\n");
            startRepl();
        }
        return 0;
    }
    #endif
    else if (argc < 4) {
        #ifdef use_readline
        #define DASHDASHREPL " [--repl]"
        #else
        #define DASHDASHREPL ""
        #endif
        fprintf(stderr, "%s %s\nUsage: %s [--help]%s %s\n", NAME,VERSION,argv[0],DASHDASHREPL,BASIC_USAGE_STRING);
        return 1;
    }



    //print all args
    #ifdef DEBUG
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }
    #endif

    // Step 1: Resolve the library path
    // For now we've delegated that call to parse_arguments




    // Step 2: Parse command-line arguments
    FunctionCallInfo* call_info = parse_arguments(argc-1, argv+1); // skip the program name
    // convert_all_arrays_to_arginfo_ptr_sized_after_parsing(call_info); <-- handled inside parse_arguments now
    

    // Step 2.5 (optional): Print the parsed function call call_info
    log_function_call_info(call_info);

    // Step 3: Invoke the specified function

    void* lib_handle = loadLibraryDirectly(call_info->library_path);

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
}
