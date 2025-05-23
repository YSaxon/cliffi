#ifndef ARG_TOOLS_H
#define ARG_TOOLS_H

#include <stdbool.h>
#include <stdbool.h>
#include <stddef.h> // For size_t
#include <stdint.h> // For fixed-width integer types

typedef enum {
    // note that we can't use number chars (eg '1') as flags without conflicting with implicit representation of negative numbers
    TYPE_CHAR = 'c',
    TYPE_SHORT = 'h',
    TYPE_INT = 'i',
    TYPE_LONG = 'l',
    TYPE_UCHAR = 'C',
    TYPE_USHORT = 'H',
    TYPE_UINT = 'I',
    TYPE_ULONG = 'L',
    TYPE_FLOAT = 'f',
    TYPE_DOUBLE = 'd',
    TYPE_STRING = 's',      // For null terminated strings (char*)
    TYPE_POINTER = 'p',     // For parsing pointers only, not an actual type
    TYPE_VOIDPOINTER = 'P', // For representing arbitrary pointers by their addresses
    TYPE_ARRAY = 'a',       // For parsing only, not an actual type
    TYPE_VOID = 'v',        // For parsing return types only
    TYPE_UNKNOWN = -1,      // For representing unknown types when parsing
    TYPE_STRUCT = 'S',
    TYPE_BOOL = 'b',        // For boolean values
} ArgType;

typedef enum {
    NOT_ARRAY, // evaluates as false
    ARRAY_STATIC_SIZE_UNSET,
    ARRAY_STATIC_SIZE,
    ARRAY_SIZE_AT_ARGNUM,
    ARRAY_SIZE_AT_ARGINFO_PTR
} arrayMode;

typedef struct ArgInfo {
    ArgType type;      // Argument type
    bool explicitType; // True if the type was explicitly specified
    union {
        char c_val;
        short s_val;
        int i_val;
        long l_val;
        unsigned char uc_val;
        unsigned short us_val;
        unsigned int ui_val;
        unsigned long ul_val;
        float f_val;
        double d_val;
        char* str_val; // For strings
        void* ptr_val; // For pointers or complex data
        bool b_val;    // For boolean values
    }* value;
    int pointer_depth;             // For pointer types, indicates how many levels of indirection
    int array_value_pointer_depth; // For array types, indicates how many levels of indirection for the value pointers
    arrayMode is_array;            // For pointer types, indicates if the pointer is an array
    size_t static_or_implied_size; // For array types, indicates the size of the array
    union {
        int argnum_of_size_t_to_be_replaced; // For array types, indicates which argument to use as the size_t, -1 = RETURN VAL, 0 = FIRST ARG, 1 = SECOND ARG, etc.
        struct ArgInfo* arginfo_of_size_t;
    } array_sizet_arg; // For array types, indicates which argument to use as the size_t, as an alternative to array_size

    struct StructInfo* struct_info; // For struct types, contains the struct info, since we can't set the ptr_val until we calculate the raw memory for the struct later
} ArgInfo;

typedef struct ArgInfoContainer {
    ArgInfo** args;
    unsigned int arg_count;
    ArgInfo* return_var; // this and the next one really belong in FunctionCallInfo, but we need to use them in the parsing functions
    int vararg_start;    // -1 if no varargs, otherwise the index of the first vararg
} ArgInfoContainer;

typedef struct StructInfo {
    struct ArgInfoContainer info;
    // possibly we should save a pointer to the struct's memory, so we can free it later
    bool is_packed;
} StructInfo;
typedef struct FunctionCallInfo {
    struct ArgInfoContainer info;
    char* library_path;
    char* function_name;
    // ArgInfo return_var;
} FunctionCallInfo;

// Function declarations
char typeToChar(ArgType type);
char* typeToString(ArgType type);
ArgType charToType(char c);
void freeFunctionCallInfo(FunctionCallInfo* info);
bool isAllDigits(const char* str);
bool isHexFormat(const char* str);
bool endsWith(const char* str, char c);
bool isFloatingPoint(const char* str);
void* hex_string_to_bytes(const char* hexStr);
void infer_arg_type_from_value(ArgInfo* arg, const char* argval);
void convert_arg_value(ArgInfo* arg, const char* argStr);
void log_function_call_info(FunctionCallInfo* info);
size_t typeToSize(ArgType type, int array_value_pointer_depth);

size_t get_size_for_arginfo_sized_array(const ArgInfo* arg);
void convert_all_arrays_to_arginfo_ptr_sized_after_parsing(ArgInfoContainer* functionInfo);
void second_pass_arginfo_ptr_sized_null_array_initialization(ArgInfoContainer* call_info);
char* trim_whitespace(char* str);

void* makePointerLevel(void* value, int pointer_depth);
void* dereferencePointerLevels(void* value, int pointer_depth);

void castArgValueToType(ArgInfo* destinationTypedArg, ArgInfo* sourceValueArg);
void set_arg_value_nullish(ArgInfo* arg);


#endif /* ARG_TOOLS_H */
