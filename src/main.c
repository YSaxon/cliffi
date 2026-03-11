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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>

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
#include <psapi.h>
#else
#include <dlfcn.h>
#endif

#ifdef __linux__
#include <libgen.h>
#endif

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <libgen.h>
#endif

#include "shims.h"

const char* NAME = "cliffi";
const char* VERSION = "v1.12.6";
const char* BASIC_USAGE_STRING = "<library> <return_typeflag> <function_name> [[-typeflag] <arg>.. [ ... <varargs>..] ]\n";

void printVariableWithArgInfo(char* varName, ArgInfo* arg);

typedef struct {
    uintptr_t start;
    uintptr_t end;
} MemoryRegion;

typedef struct {
    uintptr_t address;
    size_t region_index;
} MemoryMatch;

static bool path_ends_with(const char* full_path, const char* suffix) {
    if (full_path == NULL || suffix == NULL) {
        return false;
    }
    size_t full_len = strlen(full_path);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > full_len) {
        return false;
    }
    return strcmp(full_path + full_len - suffix_len, suffix) == 0;
}

static unsigned char* parse_search_bytes(const char* bytestring, size_t* out_len) {
    if (bytestring == NULL || out_len == NULL) {
        raiseException(1, "Error: Invalid bytestring input\n");
    }
    if (strncmp(bytestring, "0x", 2) == 0 || strncmp(bytestring, "0X", 2) == 0) {
        size_t hex_len = strlen(bytestring + 2);
        if (hex_len == 0 || hex_len % 2 != 0) {
            raiseException(1, "Error: Hex bytestring must contain an even number of hex chars\n");
        }
        *out_len = hex_len / 2;
        return hex_string_to_bytes(bytestring);
    }
    *out_len = strlen(bytestring);
    if (*out_len == 0) {
        raiseException(1, "Error: Empty bytestring not allowed\n");
    }
    unsigned char* out = malloc(*out_len);
    if (out == NULL) {
        raiseException(1, "Error: Failed to allocate bytestring buffer\n");
    }
    memcpy(out, bytestring, *out_len);
    return out;
}

static void append_region(MemoryRegion** regions, size_t* region_count, size_t* region_capacity, uintptr_t start, uintptr_t end) {
    if (end <= start) {
        return;
    }
    if (*region_count >= *region_capacity) {
        *region_capacity = *region_capacity == 0 ? 16 : *region_capacity * 2;
        *regions = realloc(*regions, *region_capacity * sizeof(MemoryRegion));
        if (*regions == NULL) {
            raiseException(1, "Error: Failed to allocate memory region list\n");
        }
    }
    (*regions)[*region_count].start = start;
    (*regions)[*region_count].end = end;
    (*region_count)++;
}

#ifdef __linux__
static size_t get_readable_regions_for_library(const char* library_path, MemoryRegion** out_regions) {
    FILE* maps = fopen("/proc/self/maps", "r");
    if (maps == NULL) {
        raiseException(1, "Error: Could not open /proc/self/maps\n");
    }
    char line[4096];
    MemoryRegion* regions = NULL;
    size_t region_count = 0;
    size_t region_capacity = 0;
    while (fgets(line, sizeof(line), maps) != NULL) {
        unsigned long start = 0;
        unsigned long end = 0;
        char perms[5] = {0};
        char mapped_path[2048] = {0};
        int fields = sscanf(line, "%lx-%lx %4s %*s %*s %*s %2047[^\n]", &start, &end, perms, mapped_path);
        if (fields < 3) {
            continue;
        }
        if (perms[0] != 'r') {
            continue;
        }
        if (fields < 4) {
            continue;
        }
        char* clean_path = mapped_path;
        while (*clean_path && isspace((unsigned char)*clean_path)) {
            clean_path++;
        }
        if (path_ends_with(clean_path, library_path)) {
            append_region(&regions, &region_count, &region_capacity, (uintptr_t)start, (uintptr_t)end);
        }
    }
    fclose(maps);
    *out_regions = regions;
    return region_count;
}
#elif defined(_WIN32)
static size_t get_readable_regions_for_library(void* lib_handle, MemoryRegion** out_regions) {
    MODULEINFO module_info;
    if (!GetModuleInformation(GetCurrentProcess(), (HMODULE)lib_handle, &module_info, sizeof(module_info))) {
        raiseException(1, "Error: Could not get module information\n");
    }
    uintptr_t module_start = (uintptr_t)module_info.lpBaseOfDll;
    uintptr_t module_end = module_start + module_info.SizeOfImage;

    MemoryRegion* regions = NULL;
    size_t region_count = 0;
    size_t region_capacity = 0;
    MEMORY_BASIC_INFORMATION mbi;
    unsigned char* addr = (unsigned char*)module_start;
    while (addr < (unsigned char*)module_end && VirtualQuery(addr, &mbi, sizeof(mbi))) {
        uintptr_t start = (uintptr_t)mbi.BaseAddress;
        uintptr_t end = start + mbi.RegionSize;
        bool readable = mbi.State == MEM_COMMIT && !(mbi.Protect & PAGE_GUARD) && !(mbi.Protect & PAGE_NOACCESS);
        if (readable) {
            uintptr_t clipped_start = start < module_start ? module_start : start;
            uintptr_t clipped_end = end > module_end ? module_end : end;
            append_region(&regions, &region_count, &region_capacity, clipped_start, clipped_end);
        }
        addr = (unsigned char*)end;
    }
    *out_regions = regions;
    return region_count;
}
#elif defined(__APPLE__)
static size_t get_readable_regions_for_library(const char* library_path, MemoryRegion** out_regions) {
    const struct mach_header* image_header = NULL;
    intptr_t image_slide = 0;
    uint32_t image_count = _dyld_image_count();
    for (uint32_t i = 0; i < image_count; i++) {
        const char* image_name = _dyld_get_image_name(i);
        if (image_name != NULL && path_ends_with(image_name, library_path)) {
            image_header = _dyld_get_image_header(i);
            image_slide = _dyld_get_image_vmaddr_slide(i);
            break;
        }
    }
    if (image_header == NULL) {
        *out_regions = NULL;
        return 0;
    }

    uintptr_t image_start = UINTPTR_MAX;
    uintptr_t image_end = 0;
    const struct load_command* cmd = (const struct load_command*)((const char*)image_header + sizeof(struct mach_header_64));
    for (uint32_t i = 0; i < ((struct mach_header_64*)image_header)->ncmds; i++) {
        if (cmd->cmd == LC_SEGMENT_64) {
            const struct segment_command_64* seg = (const struct segment_command_64*)cmd;
            uintptr_t start = (uintptr_t)(seg->vmaddr + image_slide);
            uintptr_t end = start + seg->vmsize;
            if (start < image_start) image_start = start;
            if (end > image_end) image_end = end;
        }
        cmd = (const struct load_command*)((const char*)cmd + cmd->cmdsize);
    }
    if (image_start >= image_end) {
        *out_regions = NULL;
        return 0;
    }

    MemoryRegion* regions = NULL;
    size_t region_count = 0;
    size_t region_capacity = 0;
    mach_vm_address_t addr = image_start;
    while (addr < image_end) {
        mach_vm_size_t size = 0;
        vm_region_basic_info_data_64_t info;
        mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object_name = MACH_PORT_NULL;
        kern_return_t kr = mach_vm_region(mach_task_self(), &addr, &size, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &count, &object_name);
        if (kr != KERN_SUCCESS) {
            break;
        }
        if (info.protection & VM_PROT_READ) {
            uintptr_t start = (uintptr_t)addr;
            uintptr_t end = start + size;
            if (end > image_start && start < image_end) {
                uintptr_t clipped_start = start < image_start ? image_start : start;
                uintptr_t clipped_end = end > image_end ? image_end : end;
                append_region(&regions, &region_count, &region_capacity, clipped_start, clipped_end);
            }
        }
        addr += size;
    }
    *out_regions = regions;
    return region_count;
}
#endif

static bool find_bytes_in_regions(const MemoryRegion* regions, size_t region_count, const unsigned char* bytes, size_t bytes_len, MemoryMatch* first_match, MemoryMatch* second_match) {
    bool found_first = false;
    for (size_t i = 0; i < region_count; i++) {
        uintptr_t start = regions[i].start;
        size_t size = regions[i].end - regions[i].start;
        if (size < bytes_len) {
            continue;
        }
        unsigned char* memory = (unsigned char*)start;
        for (size_t offset = 0; offset <= size - bytes_len; offset++) {
            if (memcmp(memory + offset, bytes, bytes_len) == 0) {
                if (!found_first) {
                    first_match->address = start + offset;
                    first_match->region_index = i;
                    found_first = true;
                } else {
                    second_match->address = start + offset;
                    second_match->region_index = i;
                    return true;
                }
            }
        }
    }
    return found_first;
}

static void dump_match_context(const MemoryRegion* region, uintptr_t match_address, size_t bytes_len) {
    const size_t max_context = 64;
    uintptr_t before = bytes_len < max_context ? bytes_len : max_context;
    uintptr_t after = max_context;
    uintptr_t dump_start = match_address > region->start + before ? match_address - before : region->start;
    uintptr_t desired_end = match_address + bytes_len + after;
    uintptr_t dump_end = desired_end < region->end ? desired_end : region->end;
    size_t dump_size = dump_end - dump_start;
    printf("Hexdump context around match at 0x%" PRIxPTR " (size=%zu):\n", match_address, dump_size);
    hexdump((void*)dump_start, dump_size);
}

void parseSetOffset(char* setOffsetCommand) {
    int argc;
    char** argv;
    tokenize(setOffsetCommand, &argc, &argv);
    if (argc != 2) {
        raiseException(1, "Error: Invalid number of arguments for set_offset\n");
        return;
    }
    char* library_name = argv[0];
    char* offset_str = argv[1];
    void* lib_handle = getOrLoadLibrary(library_name);
    void* offset = getAddressFromAddressStringOrNameOfCoercableVariable(offset_str);
    storeOffsetForLibLoadedAtAddress(lib_handle, offset);
    printf("Stored offset for %s as %p\n", library_name, offset);
}

void parseSearchOffset(char* searchOffsetCommand) {
    int argc;
    char** argv;
    tokenize(searchOffsetCommand, &argc, &argv);
    if (argc < 3 || argc > 4) {
        raiseException(1, "Error: Invalid number of arguments for search_offset\n");
        return;
    }

    bool hasVar = argc == 4;
    char* var_name = hasVar ? argv[0] : NULL;
    char* library_name = argv[0 + hasVar];
    char* bytestring = argv[1 + hasVar];
    char* provided_addr_str = argv[2 + hasVar];
    uintptr_t provided_addr = (uintptr_t)getAddressFromAddressStringOrNameOfCoercableVariable(provided_addr_str);

    void* lib_handle = getOrLoadLibrary(library_name);
    char* resolved_library_path = resolve_library_path(library_name);

    size_t bytes_len = 0;
    unsigned char* bytes = parse_search_bytes(bytestring, &bytes_len);

    MemoryRegion* regions = NULL;
#ifdef _WIN32
    size_t region_count = get_readable_regions_for_library(lib_handle, &regions);
#else
    size_t region_count = get_readable_regions_for_library(resolved_library_path, &regions);
#endif
    if (region_count == 0) {
        free(bytes);
        raiseException(1, "Error: Could not find readable mapped regions for library %s\n", library_name);
    }

    MemoryMatch first_match = {0};
    MemoryMatch second_match = {0};
    bool found = find_bytes_in_regions(regions, region_count, bytes, bytes_len, &first_match, &second_match);
    if (!found) {
        free(bytes);
        free(regions);
        raiseException(1, "Error: Did not find the requested bytestring in readable regions of %s\n", library_name);
    }

    printf("Found first bytestring match at 0x%" PRIxPTR "\n", first_match.address);
    dump_match_context(&regions[first_match.region_index], first_match.address, bytes_len);

    if (second_match.address != 0) {
        printf("Found second bytestring match at 0x%" PRIxPTR "\n", second_match.address);
        dump_match_context(&regions[second_match.region_index], second_match.address, bytes_len);
        free(bytes);
        free(regions);
        raiseException(1, "Error: Bytestring is not unique in %s. Please provide a more specific string.\n", library_name);
    }

    ptrdiff_t offset = (ptrdiff_t)(first_match.address - provided_addr);
    printf("Calculation: 0x%" PRIxPTR " - 0x%" PRIxPTR " = %p\n", first_match.address, provided_addr, (void*)offset);
    storeOffsetForLibLoadedAtAddress(lib_handle, (void*)offset);
    if (hasVar) {
        ArgInfo* offsetArg = getPVar((void*)offset);
        setVar(var_name, offsetArg);
        printVariableWithArgInfo(var_name, offsetArg);
    }

    free(bytes);
    free(regions);
}





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

void print_function_return(FunctionCallInfo* call_info){
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
            printf("Parsed func '%s' as relative address to the library offset, 0x%" PRIxPTR "\n", function_name, (uintptr_t)address_offset_relative_to_lib);
            return address_offset_relative_to_lib;
        } else {
            raiseException(1,  "Error: Could not find a stored offset for your library. Try again after running calculate_offset\n");
            return NULL;
        }
    }

    void* addressDirectly = tryGetAddressFromAddressStringOrNameOfCoercableVariable(function_name);
    if (addressDirectly != NULL) {
        printf("Parsed func '%s' as an absolute address -> 0x%" PRIxPTR "\n", function_name, (uintptr_t)addressDirectly);
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
    printf("Calculation: dlsym(%s,%s)=%p; %p - %p = %p\n", libraryName, symbolName, symbol_handle, symbol_handle, address, (void*)offset);

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
                       "  calculate_offset [<variable>] <library> <symbol> <address>:"
                       "      Calculate memory offset by comparing the address of a known symbol [and store in var]\n"
                       "  set_offset <library> <offset>: Set memory offset for a library directly\n"
                       "  search_offset [<variable>] <library> <bytestring> <address>:"
                       "      Search readable library pages for bytestring, verify uniqueness, and store computed offset\n"
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
                printf("Closing Library: %s\n",resolvedPath);
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
            } else if (strncmp(command, "set_offset ", 11) == 0) {
                parseSetOffset(command + 11);
            } else if (strncmp(command, "search_offset ", 14) == 0) {
                parseSearchOffset(command + 14);
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

#ifndef CLIFFI_UNIT_TESTING // don't defined main() when compiling for unit tests to avoid a collision
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
                TRY
                parseREPLCommand(command);
                CATCHALL
                    printException();
                    if (isTestEnvExit1OnFail) exit(1);
                    fprintf(stderr, "Restarting REPL...\n");
                END_TRY
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
