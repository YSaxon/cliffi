#include "argparser.h"
#include "invoke_handler.h"
#include "library_manager.h"
#include "library_path_resolver.h"
#include "parse_address.h"
#include "return_formatter.h"
#include "types_and_utils.h"
#include "var_map.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "exception_handling.h"

#include "tokenize.h"
#if  !defined(_WIN32) && !defined(_WIN64)
#include <readline/history.h>
#include <readline/readline.h>
#include <sys/wait.h> // same as above
#include <unistd.h>   // only used for forking for --repltest repl test harness mode
#endif



#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "shims.h"

const char* NAME = "cliffi";
const char* VERSION = "v1.10.22";
const char* BASIC_USAGE_STRING = "<library> <return_typeflag> <function_name> [[-typeflag] <arg>.. [ ... <varargs>..] ]\n";



bool isTestEnvExit1OnFail = false;


void print_usage(char* argv0) {
    printf("%s %s\n", NAME, VERSION);
    printf("Usage: %s %s\n", argv0, BASIC_USAGE_STRING);
    printf("  [--help]         Print this help message\n"
           "  [--repl]         Start the REPL\n"
           "  <library>        The path to the shared library containing the function to invoke\n"
           "                   or the name of the library if it is in the system path\n"
           "  <typeflag>       The type of the return value of the function to invoke\n"
           "                   v for void, i for int, s for string, etc\n"
           "  <function_name>  The name of the function to invoke\n"
           "  [-<typeflag>] <arg> The argument values to pass to the function\n"
           "                   Types will be inferred if not prefixed with flags\n"
           "                   Flags look like -i for int, -s for string, etc\n"
           "  ...              Mark the position of varargs in the function signature if applicable\n");
    printf("\n"
           "  BASIC EXAMPLES:\n");
    printf("         %s libexample.so i addints 3 4\n", argv0);
    printf("         %s path/to/libexample.so v dofoo\n", argv0);
    printf("         %s ./libexample.so s concatstrings -s hello -s world\n", argv0);
    printf("         %s libexample.so s concatstrings hello world\n", argv0);
    printf("         %s libexample.so d multdoubles -d 1.5 1.5d\n", argv0);
    printf("         %s libc.so i printf 'Here is a number: %%.3f' ... 4.5", argv0);
    printf("\n");
    printf("  TYPES:\n"
           "     The primitive typeflags are:\n");
    printf("       %c for void, only allowed as a return type, and does not accept prefixes\n", TYPE_VOID);
    printf("       %c for char\n", TYPE_CHAR);
    printf("       %c for short\n", TYPE_SHORT);
    printf("       %c for int\n", TYPE_INT);
    printf("       %c for long\n", TYPE_LONG);
    printf("       %c for unsigned char\n", TYPE_UCHAR);
    printf("       %c for unsigned short\n", TYPE_USHORT);
    printf("       %c for unsigned int\n", TYPE_UINT);
    printf("       %c for unsigned long\n", TYPE_ULONG);
    printf("       %c for float, can also be specified by suffixing the value with f\n", TYPE_FLOAT);
    printf("       %c for double, can also be specified by suffixing the value with d\n", TYPE_DOUBLE);
    printf("       %c for cstring (ie null terminated char*)\n", TYPE_STRING);
    printf("       %c for arbitrary pointer (ie void*) specified by address\n", TYPE_VOIDPOINTER);
    printf("\n");
    printf("  POINTERS AND ARRAYS AND STRUCTS:\n"
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
    printf("     * For a function: int return_buffer(char** outbuff) which returns size\n");
    printf("     %s libexample.so v return_buffer -past2 NULL -pi 0\n", argv0);
    printf("     * Or alternatively if it were: void return_buffer(char** outbuff, size_t* outsize)\n");
    printf("     %s libexample.so i return_buffer -past0 NULL\n", argv0);
    printf("     * For a function: int add_all_ints(int** nums, size_t size) which returns sum\n");
    printf("     %s libexample.so i add_all_ints -ai 1,2,3,4,5 -i 5\n", argv0);
    printf("\n");
    printf("   STRUCTS:\n"
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
    printf("      %s libexample.so v print_struct -S: 3 \"hello world\" :S\n", argv0);
    printf("      * For a function: struct mystruct return_struct(int x, char* s)\n");
    printf("      %s libexample.so S: i s :S 5 \"hello world\"\n", argv0);
    printf("      * For a function: modifyStruct(struct mystruct* s)\n");
    printf("      %s libexample.so v modifyStruct -pS: 3 \"hello world\" :S\n", argv0);
    printf("\n");
    printf("  VARARGS:\n"
           "     If a function takes varargs the position of the varargs should be specified with the `...` flag\n"
           "     The `...` flag may sometimes be the first arg if the function takes all varargs, or the last for a function that takes varargs where none are being passed\n"
           "     The varargs themselves are the same as any other function args and can be with or without typeflags\n"
           "     (Floats and types shorter than int will be upgraded for you automatically)\n"
           "    VARARGS EXAMPLES:\n");
    printf("      %s libc.so i printf 'Hello %%s, your number is: %%.3f' ... bob 4.5\n", argv0);
    printf("      %s libc.so i printf 'This is just a static string' ... \n", argv0);
    printf("      %s some_lib.so v func_taking_all_varargs ... -i 3 -s hello\n", argv0);
}

int invoke_and_print_return_value(FunctionCallInfo* call_info, void (*func)(void)) {
    int invoke_result = invoke_dynamic_function(call_info, func);
    if (invoke_result != 0) {
        fprintf(stderr, "Error: Function invocation failed\n");
    } else {

        setCodeSectionForSegfaultHandler("invoke_and_print_return_value : while printing values");

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
    unsetCodeSectionForSegfaultHandler();

    return invoke_result;
}

void* loadFunctionHandle(void* lib_handle, const char* function_name) {
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


void printVariableWithArgInfo(char* varName, ArgInfo* arg) {
    format_and_print_arg_type(arg);
    printf(" %s = ", varName);
    format_and_print_arg_value(arg);
    printf("\n");
}

void parsePrintVariable(char* varName) {
    ArgInfo* arg = getVar(varName);
    if (arg == NULL) {
        raiseException(1,  "Error printing var: Variable %s not found.\n", varName);
    } else {
        printVariableWithArgInfo(varName, arg);
    }
}

void parseStoreToMemoryWithAddressAndValue(char* addressStr, int varValueCount, char** varValues) {

    if (addressStr == NULL || strlen(addressStr) == 0) {
        raiseException(1,  "Memory address cannot be empty.\n");
    }
    if (varValues == NULL || varValueCount == 0 || strlen(varValues[0]) == 0) {
        raiseException(1,  "Variable value cannot be empty.\n");
    }

    void* destAddress = getAddressFromAddressStringOrNameOfCoercableVariable(addressStr);

    int args_used = 0;
    ArgInfo* arg = parse_one_arg(varValueCount, varValues, &args_used, false);
    if (args_used + 1 != varValueCount) {
        free(arg);
        raiseException(1,  "Invalid variable value. Parser failed to consume entire line.\n");
        return;
    }

    if (arg->type == TYPE_STRUCT) {
        // temporarily store the struct in a raw value and copy it to the destination address
        //  arg->value->ptr_val = make_raw_value_for_struct(arg, false);
        void* raw_struct = make_raw_value_for_struct(arg, false);
        size_t size = arg->pointer_depth == 0 ? get_size_of_struct(arg) : sizeof(void*);
        memcpy(destAddress, raw_struct, size);
    } else if (arg->is_array) { // if it's an array then the pointer to the raw value is stored in the ptr_val field
        if (arg->array_value_pointer_depth > 0) {
            memcpy(destAddress, arg->value->ptr_val, sizeof(void*));
        } else {
            size_t array_len = get_size_for_arginfo_sized_array(arg);
            memcpy(destAddress, arg->value->ptr_val, array_len * typeToSize(arg->type, arg->array_value_pointer_depth));
        }
    } else {
        memcpy(destAddress, arg->value, typeToSize(arg->type, arg->array_value_pointer_depth));
    }
    // #define HEX_DIGITS (int)(2 * sizeof(void*))
    // printf("*( (void*) 0x%0*" PRIxPTR ") = ",HEX_DIGITS,(uintptr_t)destAddress);
    printf("*( (void*) 0x%" PRIxPTR ") = ", (uintptr_t)destAddress);
    format_and_print_arg_type(arg);
    printf(" ");
    format_and_print_arg_value(arg);
    printf("\n");
}

ArgInfo* parseLoadMemoryToArgWithType(char* addressStr, int typeArgc, char** typeArgv) {
    if (addressStr == NULL || strlen(addressStr) == 0) {
        raiseException(1,  "Memory address cannot be empty.\n");
    }
    if (typeArgv == NULL || typeArgc == 0 || strlen(typeArgv[0]) == 0) {
        raiseException(1,  "Variable type cannot be empty.\n");
    }

    int args_used = 0;
    ArgInfo* arg = parse_one_arg(typeArgc, typeArgv, &args_used, true);
    if (args_used + 1 != typeArgc) {
        free(arg);
        raiseException(1,  "Invalid type. Specify it as if it were a return type (ie types only, no dashes).\n");
        return NULL;
    }

    void* sourceAddress = getAddressFromAddressStringOrNameOfCoercableVariable(addressStr);

    // possibly we also want to check if its an array and if so copy it's address instead of the value since we use pointer types for arrays (as if it was inside a struct)
    if (arg->is_array) {
        arg->value->ptr_val = sourceAddress;
    } else if (arg->type == TYPE_STRUCT) {
        fix_struct_pointers(arg, sourceAddress);
    } else {
        memcpy(arg->value, sourceAddress, typeToSize(arg->type, arg->pointer_depth));
    }
    return arg;
}

void parseDumpMemoryWithAddressAndType(char* addressStr, int varValueCount, char** varValues) {
    ArgInfo* arg = parseLoadMemoryToArgWithType(addressStr, varValueCount, varValues);
    printf("(");
    format_and_print_arg_type(arg);
    printf("*) %s = ", addressStr);
    format_and_print_arg_type(arg);
    printf(" ");
    format_and_print_arg_value(arg);
    printf("\n");
    free(arg);
}

void parseSetVariableWithNameAndValue(char* varName, int varValueCount, char** varValues) {

    if (varName == NULL || strlen(varName) == 0) {
        raiseException(1,  "Variable name cannot be empty.\n");
    } else if (varValues == NULL || varValueCount == 0 || strlen(varValues[0]) == 0) {
        raiseException(1,  "Variable value cannot be empty.\n");
    }

    if (strlen(varName) == 1 && charToType(*varName) != TYPE_UNKNOWN) {
        raiseException(1,  "Variable name cannot be a character used in parsing types, such as %s which is used for %s\n", varName, typeToString(charToType(*varName)));
    } else if (*varName == '-') {
        raiseException(1,  "Variable names cannot start with a dash.\n");
    } else if (isAllDigits(varName) || isHexFormat(varName) || isFloatingPoint(varName)) {
        raiseException(1,  "Variable names cannot be a number.\n");
    }

    int args_used = 0;
    ArgInfo* arg = parse_one_arg(varValueCount, varValues, &args_used, false);
    if (args_used + 1 != varValueCount) {
        free(arg);
        raiseException(1,  "Invalid variable value (parser failed to consume entire value line)\n");
        return;
    }
    printVariableWithArgInfo(varName, arg);
    setVar(varName, arg);
    // Should we check if the variable already existed and free the previous value? Or maybe keep a reference count?
}

void executeREPLCommand(char* command) {
    int argc;
    char** argv;
    if (tokenize(command, &argc, &argv) != 0) {
        raiseException(1,  "Error: Tokenization failed for command\n");
    }

    // syntactic sugar for set <var> <value> and print <var>
    if (argc == 1) {
        if (isHexFormat(argv[0])) {
            raiseException(1,  "You can't print a memory address with specifying a type, try again with: dump <type> %s\n", argv[0]);
        } else {
            parsePrintVariable(argv[0]);
        }
        return;
    } else if (argc >= 3 && strcmp(argv[1], "=") == 0) {
        if (isHexFormat(argv[0])) {
            parseStoreToMemoryWithAddressAndValue(argv[0], argc - 2, argv + 2);
        } else {
            parseSetVariableWithNameAndValue(argv[0], argc - 2, argv + 2);
        }
        return;
    }

    if (argc < 3) {
        raiseException(1,  "Invalid command '%s'. Type 'help' for assistance.\n", command);
    }
    FunctionCallInfo* call_info = parse_arguments(argc, argv);
    log_function_call_info(call_info);
    void* lib_handle = getOrLoadLibrary(call_info->library_path);
    if (lib_handle == NULL) {
        raiseException(1,  "Failed to load library: %s\n", call_info->library_path);
    }
    void* func = loadFunctionHandle(lib_handle, call_info->function_name);

    int invoke_result = invoke_and_print_return_value(call_info, func);
    if (invoke_result != 0) {
        raiseException(1,  "Error: Function invocation failed\n");
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

void discard_equals_but_warn_if_present(char*** argv, int* argc) {
    if (*argc > 0 && strcmp((*argv)[0], "=") == 0){
        fprintf(stderr, "Warning: '=' sign is not necessary when explicitly specifying the command and will be ignored.\n");
        (*argv)++;
        (*argc)--;
    }
}

void parseSetVariable(char* varCommand) {
    int argc;
    char** argv;
    tokenize(varCommand, &argc, &argv);
    // <var> <value>
    if (argc < 2) {
        raiseException(1,  "Error: Invalid number of arguments for set\n");
        return;
    }
    char* varName = argv[0];      // first argument is the variable name
    int value_args = argc - 1;    // all but the first argument
    char** value_argv = argv + 1; // starts after the first argument
    discard_equals_but_warn_if_present(&value_argv, &value_args);
    parseSetVariableWithNameAndValue(varName, value_args, value_argv);
}

void parseStoreToMemory(char* memCommand) {
    int argc;
    char** argv;
    tokenize(memCommand, &argc, &argv);
    // <address> <value>
    if (argc < 2) {
        raiseException(1,  "Error: Invalid number of arguments for storemem\n");
        return;
    }
    char* address = argv[0];      // first argument is the address
    int value_args = argc - 1;    // all but the first argument
    char** value_argv = argv + 1; // starts after the first argument
    discard_equals_but_warn_if_present(&value_argv, &value_args);
    parseStoreToMemoryWithAddressAndValue(address, value_args, value_argv);
}

void parseDumpMemory(char* memCommand) {
    int argc;
    char** argv;
    tokenize(memCommand, &argc, &argv);
    // <type> <address>
    if (argc < 2) {
        raiseException(1,  "Error: Invalid number of arguments for dumpmem\n");
        return;
    }
    char* address = argv[argc - 1]; // last argument is the address
    int type_args = argc - 1;       // all but the last argument
    char** type_argv = argv;        // starts at the first argument
    parseDumpMemoryWithAddressAndType(address, type_args, type_argv);
}

void parseLoadMemoryToVar(char* loadCommand) {
    int argc;
    char** argv;
    tokenize(loadCommand, &argc, &argv);
    // <var> <type> <address>
    if (argc < 3) {
        raiseException(1,  "Error: Invalid number of arguments for loadmem\n");
        return;
    }
    char* varName = argv[0];
    char* address = argv[argc - 1]; // last argument is the address
    int type_args = argc - 2;       // all but the first and last argument
    char** type_argv = argv + 1;    // starts after the first argument
    discard_equals_but_warn_if_present(&type_argv, &type_args);
    ArgInfo* arg = parseLoadMemoryToArgWithType(address, type_args, type_argv);
    printVariableWithArgInfo(varName, arg);
    setVar(varName, arg);
}

void parseCalculateOffset(char* calculateCommand) {
    int argc;
    char** argv;
    tokenize(calculateCommand, &argc, &argv);
    // <var> <library> <symbol> <address>
    if (argc < 4) {
        raiseException(1,  "Error: Invalid number of arguments for calculate_offset\n");
        return;
    }
    char* varName = argv[0];
    char* libraryName = argv[1];
    char* symbolName = argv[2];
    char* addressStr = argv[3];
    void* address = getAddressFromAddressStringOrNameOfCoercableVariable(addressStr);
    // possibly should pass this through the parser to get the actual path of the library
    void* lib_handle = getOrLoadLibrary(libraryName);
    void* symbol_handle = loadFunctionHandle(lib_handle, symbolName);
    uintptr_t symbol_address = (uintptr_t)symbol_handle;
    #if defined(__arm__)
    address = (void*)((uintptr_t) address & ~1);    // clear the thumb bit
    symbol_address &= ~1;                           // clear the thumb bit
    #endif
    if (symbol_address < (uintptr_t)address) {
        raiseException(1,  "Error: Calculated offset is negative. This is likely an error and the variable probably won't work.\n");
    }
    ptrdiff_t offset = symbol_address - (uintptr_t)address; // maybe technically we should use ptrdiff_t instead but it's unlikely that the offset would be negative
    printf("Calculation: dlsym(%s,%s)=%p; %p - %p = %p\n", libraryName, symbolName, symbol_handle, symbol_handle, address, (void*)offset);
    // it's a little convoluted but we'll just convert to a string and call a func to convert it back
    char offsetStr[32];
    snprintf(offsetStr, sizeof(offsetStr), "%zu", offset);
    char* varValues[2];
    varValues[0] = "-P";
    varValues[1] = offsetStr;
    parseSetVariableWithNameAndValue(varName, 2, varValues);
}

void parseHexdump(char* hexdumpCommand) {
    int argc;
    char** argv;
    tokenize(hexdumpCommand, &argc, &argv);
    // <address> <size>
    if (argc < 2) {
        raiseException(1,  "Error: Invalid number of arguments for hexdump\n");
        return;
    }
    char* addressStr = argv[0];
    char* sizeStr = argv[1];
    void* address = getAddressFromAddressStringOrNameOfCoercableVariable(addressStr);
    if (address==NULL) {
        raiseException(1,  "Error: Invalid address for hexdump\n");
        return;
    }
    size_t size = strtoul(sizeStr, NULL, 0);
    hexdump(address, size);
}



int parseREPLCommand(char* command){
        command = trim_whitespace(command);
        if (strlen(command) > 0) {
            if (strcmp(command, "quit") == 0 || strcmp(command, "exit") == 0) {
                closeAllLibraries();
                return 1;
            } else if (strcmp(command, "help") == 0) {
                printf("Running a command:\n");
                printf("  %s\n", BASIC_USAGE_STRING);
                printf("Documentation:\n"
                       "  help: Print this help message\n"
                       "  docs: Print the cliffi docs\n"
                       "Variables:\n"
                       "  set <var> <value>: Set a variable. Alternate form: <var> = <value>\n"
                       "  print <var>: Print the value of a variable. Alternate form: <var>\n"
                       "Memory Management:\n"
                       "  store <address> <value>: Set the value of a memory address\n"
                       "  dump <type> <address>: Print the value at a memory address\n"
                       "  load <var> <type> <address>: Load the value at a memory address into a variable\n"
                       "  calculate_offset <variable> <library> <symbol> <address>:"
                       "      Calculate a memory offset by comparing the address of a known symbol\n"
                       "  hexdump <address> <size>: Print a hexdump of memory\n"
                       "Shared Library Management:\n"
                       "  list: List all opened libraries\n"
                       "  close <library>: Close the specified library\n"
                       "  closeall: Close all opened libraries\n"
                       "Shell commands:\n"
                       "  !<command>: Run a shell command\n"
                       "  shell: Drop into an interactive shell\n"
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
                parseSetVariable(command + 4);
            } else if (strncmp(command, "print ", 6) == 0) {
                parsePrintVariable(command + 6);
            } else if (strncmp(command, "store ", 6) == 0) {
                parseStoreToMemory(command + 6);
            } else if (strncmp(command, "dump ", 5) == 0) {
                parseDumpMemory(command + 5);
            } else if (strncmp(command, "load ", 5) == 0) {
                parseLoadMemoryToVar(command + 5);
            } else if (strncmp(command, "calculate_offset ", 17) == 0) {
                parseCalculateOffset(command + 17);
            } else if (strncmp(command, "hexdump ", 8) == 0) {
                parseHexdump(command + 8); // could also be done by dump aC<size> <address>
            } else if (command[0] == '!') {
                system(command + 1); // could also be done via libc.so v system "<command>" but this is more direct and convenient
            } else if (strcmp(command, "shell") == 0) {
                // if $SHELL is set, run that
                if (getenv("SHELL") != NULL) {
                    system(getenv("SHELL"));
                } else {
                    fprintf(stderr, "Warning: SHELL environment variable not set. Running an unprefixed 'sh -i'\n");
                    system("sh -i");
                }
            } else {
                executeREPLCommand(command); // also handles alternate forms of set (<varname>) and print (<varname> = <value>)
            }
        }
        return 0;
}



void startRepl() {

    char* command;

    while ((command = readline("> ")) != NULL) {
        int breakRepl = 0;
        TRY
        if (strlen(command) > 0) {
            // fprintf(stderr, "Command: %s\n", command);
            HIST_ENTRY* last_command = history_get(history_length);
            if (last_command == NULL || strcmp(command, last_command->line) != 0) {
                add_history(command);
                write_history(".cliffi_history");
            }
        breakRepl = parseREPLCommand(command);
        }
        free(command);
        CATCHALL
            printException();
            if (isTestEnvExit1OnFail) exit(1);
            fprintf(stderr, "Restarting REPL...\n");
        END_TRY
        if (breakRepl) break;
    }
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
        printf("Running cliffi init file at %s\n", full_path);
        char* line = NULL;
        size_t len = 0;
        ssize_t read;
        while ((read = getline(&line, &len, file)) != -1) {
            if (line[read - 1] == '\n') {
                line[read - 1] = '\0';
            }
            int breakRepl = parseREPLCommand(line);
            if (breakRepl) break;
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

int main(int argc, char* argv[]) {
    setbuf(stdout, NULL); // disable buffering for stdout
    setbuf(stderr, NULL); // disable buffering for stderr
    main_method_install_exception_handlers();

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
            isTestEnvExit1OnFail = true; // in general we want to exit on fail in test mode so that mistakes in the test script are caught
        }
        #if !defined(_WIN32) && !defined(_WIN64)
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

        if (pid == 0) {                    // Child process
            close(pipefd[1]);              // Close write end of pipe
            dup2(pipefd[0], STDIN_FILENO); // Redirect STDIN to read from pipe
            close(pipefd[0]);              // Close read end, not needed anymore
            goto replmode;
        } else {              // Parent process
            close(pipefd[0]); // Close the write end of the pipe
            for (int i = 2; i < argc; i++) {
                write(pipefd[1], argv[i], strlen(argv[i]));
                write(pipefd[1], " ", 1);
            }
            close(pipefd[1]); // Close the write end of the pipe
            int status;
            waitpid(pid, &status, 0);
            exit(WEXITSTATUS(status));
        }
#else // a simpler version for windows
        checkAndRunCliffiInits();
        //just feed each line to the repl execute func directly, by concatenating inputs after arg[2] except splitting for newline chars
        char* command = malloc(1024);
        command[0] = '\0'; // initialize the command
        for (int i = 2; i < argc; i++) {
            bool isNewLine = strcmp(argv[i], "\n") == 0;
            bool isFinalArg = argc - 1 == i;
            if (!isNewLine || isFinalArg ) {
                strcat(command, argv[i]);
                strcat(command, " ");
            }
            if (isNewLine || isFinalArg ){
                command[strlen(command) - 1] = '\0'; // remove the trailing space
                printf("Executing \"%s\"\n", command);
                parseREPLCommand(command);
                command[0] = '\0'; // reset the command
            }
        }
        return(0);
        #endif
    } else if (argc > 1 && strcmp(argv[1], "--repl") == 0)
    replmode: {
        checkAndRunCliffiInits();
        // rl_completion_entry_function = (Function*)cliffi_completion;
        rl_bind_key('\t', rl_complete);
        using_history();
        read_history(".cliffi_history");
        fprintf(stderr, "cliffi %s. Starting REPL... Type 'help' for assistance. Type 'exit' to quit:\n", VERSION);

        // Start the REPL
        startRepl();

        return 0;
    }

        else if (argc < 4) {
#define DASHDASHREPL " [--repl]"
            fprintf(stderr, "%s %s\nUsage: %s [--help]%s %s\n", NAME, VERSION, argv[0], DASHDASHREPL, BASIC_USAGE_STRING);
            return 1;
        }

// print all args
#ifdef DEBUG
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }
#endif

    // run .cliffi_init file if exists
    checkAndRunCliffiInits();


    // Step 1: Resolve the library path
    // For now we've delegated that call to parse_arguments

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
}
