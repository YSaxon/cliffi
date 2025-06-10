#include "repl.h"
#include "argparser.h"
#include "invoke_handler.h"
#include "library_manager.h"
#include "library_path_resolver.h"
#include "parse_address.h"
#include "return_formatter.h"
#include "types_and_utils.h"
#include "var_map.h"
#include "exception_handling.h"
#include "tokenize.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#if !defined(_WIN32) && !defined(_WIN64)
#include <readline/history.h>
#include <readline/readline.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

extern const char* NAME;
extern const char* VERSION;
extern const char* BASIC_USAGE_STRING;
extern bool isTestEnvExit1OnFail;

// Forward declarations
void* loadFunctionHandle(void* lib_handle, const char* function_name);
int invoke_and_print_return_value(FunctionCallInfo* call_info, void (*func)(void));
void print_usage(char* argv0);

void printVariableWithArgInfo(char* varName, ArgInfo* arg) {
    format_and_print_arg_type(arg);
    fprintf(stdout, " %s = ", varName);
    format_and_print_arg_value(arg);
    fprintf(stdout, "\n");
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

    int extra_args_used = 0;
    ArgInfo* arg = parse_one_arg(varValueCount, varValues, &extra_args_used, false);
    if (extra_args_used + 1 != varValueCount) {
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
    // fprintf(stdout, "*( (void*) 0x%0*" PRIxPTR ") = ",HEX_DIGITS,(uintptr_t)destAddress);
    fprintf(stdout, "*( (void*) 0x%" PRIxPTR ") = ", (uintptr_t)destAddress);
    format_and_print_arg_type(arg);
    fprintf(stdout, " ");
    format_and_print_arg_value(arg);
    fprintf(stdout, "\n");
}

ArgInfo* parseLoadMemoryToArgWithType(char* addressStr, int typeArgc, char** typeArgv) {
    if (addressStr == NULL || strlen(addressStr) == 0) {
        raiseException(1,  "Memory address cannot be empty.\n");
    }
    if (typeArgv == NULL || typeArgc == 0 || strlen(typeArgv[0]) == 0) {
        raiseException(1,  "Variable type cannot be empty.\n");
    }

    int extra_args_used = 0;
    ArgInfo* arg = parse_one_arg(typeArgc, typeArgv, &extra_args_used, true);
    if (extra_args_used + 1 != typeArgc) {
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
    fprintf(stdout, "(");
    format_and_print_arg_type(arg);
    fprintf(stdout, "*) %s = ", addressStr);
    format_and_print_arg_type(arg);
    fprintf(stdout, " ");
    format_and_print_arg_value(arg);
    fprintf(stdout, "\n");
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

    int extra_args_used = 0;
    ArgInfo* arg = parse_one_arg(varValueCount, varValues, &extra_args_used, false);
    if (extra_args_used + 1 != varValueCount) {
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
    // [<var>] <library> <symbol> <address>
    if (argc < 3 || argc > 4) {
        raiseException(1,  "Error: Invalid number of arguments for calculate_offset\n");
        return;
    }

    bool hasVar = argc == 4;
    char* varName = hasVar ? argv[0] : NULL;
    char* libraryName = argv[0 + hasVar];
    char* symbolName =  argv[1 + hasVar];
    char* addressStr =  argv[2 + hasVar];
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
    fprintf(stdout, "Calculation: dlsym(%s,%s)=%p; %p - %p = %p\n", libraryName, symbolName, symbol_handle, symbol_handle, address, (void*)offset);

    if (hasVar) {
        ArgInfo* offsetArg = getPVar((void*)offset);
        setVar(varName, offsetArg);
        printVariableWithArgInfo(varName, offsetArg);
    }

    storeOffsetForLibLoadedAtAddress(lib_handle, (void*)offset);
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
                fprintf(stdout, "Running a command:\n");
                fprintf(stdout, "  %s\n", BASIC_USAGE_STRING);
                fprintf(stdout, "Documentation:\n"
                       "  help: Print this help message\n"
                       "  docs: Print the cliffi docs\n"
                       "Variables:\n"
                       "  set <var> <value>: Set a variable. Alternate form: <var> = <value>\n"
                       "  print <var>: Print the value of a variable. Alternate form: <var>\n"
                       "Memory Management:\n"
                       "  store <address> <value>: Set the value of a memory address\n"
                       "  dump <type> <address>: Print the value at a memory address\n"
                       "  load <var> <type> <address>: Load the value at a memory address into a variable\n"
                       "  calculate_offset [<variable>] <library> <symbol> <address>:"
                       "      Calculate memory offset by comparing the address of a known symbol [and store in var]\n"
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
                char* resolvedPath = resolve_library_path(libraryName);
                fprintf(stdout, "Closing Library: %s\n",resolvedPath);
                closeLibrary(resolvedPath);
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

void startInternalServerRepl(){

    char* command;

    while ((fprintf(stderr,"<WAITING FOR NEXT REPL COMMAND>\n") && (command = readline("")) != NULL)) {
        int breakRepl = 0;
        TRY
        if (strlen(command) > 0) {
            // fprintf(stderr, "Command: %s\n", command);
            HIST_ENTRY* last_command = history_get(history_length);
            if (last_command == NULL || strcmp(command, last_command->line) != 0) {
                add_history(command);
                write_history(".cliffi_history");
            }
        fprintf(stderr, "COMMAND `%s` RECEIVED\n", command);
        breakRepl = parseREPLCommand(command);
        fprintf(stderr, "COMMAND `%s` RESPONSE FINISHED\n", command);
        }
        CATCHALL
        fprintf(stderr, "EXCEPTION THROWN DURING EXECUTION OF `%s`. PRINTING BELOW\n", command);
        printException();
        if (isTestEnvExit1OnFail)
        {
            fprintf(stderr, "EXIT ON EXCEPTION OPTION WAS SET. EXITING REPL\n");
            exit(1);
        }
        fprintf(stderr, "COMMAND `%s` RESPONSE FINISHED AFTER EXCEPTION\n", command);
        END_TRY
        if (breakRepl) break;
        else free(command);
    }
    fprintf(stderr, "EXITING REPL AFTER `%s`\n", command);
    free(command);
}