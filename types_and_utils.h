#ifndef ARG_TOOLS_H
#define ARG_TOOLS_H

#include <stdbool.h>
#include <stdint.h> // For fixed-width integer types
#include <stddef.h>  // For size_t


typedef enum {
    //note that we can't use number chars (eg '1') as flags without conflicting with implicit representation of negative numbers
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
    TYPE_STRING = 's',  // For null terminated strings (char*)
    TYPE_POINTER = 'p', // For parsing pointers only, not an actual type
    TYPE_ARRAY = 'a',   // For parsing only, not an actual type
    TYPE_VOID = 'v',    // For parsing return types only
    TYPE_UNKNOWN = -1   // For representing unknown types when parsing
} ArgType;


typedef enum{
    NOT_ARRAY, // evaluates as false
    ARRAY_STATIC_SIZE_UNSET,
    ARRAY_STATIC_SIZE,
    ARRAY_SIZE_AT_ARGNUM,
    ARRAY_SIZE_AT_ARGINFO_PTR
} arrayMode;

typedef struct ArgInfo {
    ArgType type;  // Argument type
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
    } value;
    int pointer_depth; // For pointer types, indicates how many levels of indirection
    arrayMode is_array; // For pointer types, indicates if the pointer is an array
    union {
        size_t static_size; // For array types, indicates the size of the array
        int argnum_of_size_t_to_be_replaced; // For array types, indicates which argument to use as the size_t, -1 = RETURN VAL, 0 = FIRST ARG, 1 = SECOND ARG, etc.
        struct ArgInfo* arginfo_of_size_t;
    } array_size; // For array types, indicates which argument to use as the size_t, as an alternative to array_size
} ArgInfo;

typedef struct FunctionCallInfo {
    char* library_path;
    char* function_name;
    ArgInfo return_var;
    ArgInfo* args;
    int arg_count;
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
size_t typeToSize(ArgType type);
char* typeToFormatSpecifier(ArgType type);
void handle_array_arginfo_conversion(ArgInfo* arg, const char* argStr);

size_t get_size_for_arginfo_sized_array(const ArgInfo* arg);
void convert_all_arrays_to_arginfo_ptr_sized_after_parsing(FunctionCallInfo* functionInfo);
void second_pass_arginfo_ptr_sized_null_array_initialization(FunctionCallInfo* call_info);

#endif /* ARG_TOOLS_H */
