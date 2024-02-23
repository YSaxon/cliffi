#ifndef ARG_TOOLS_H
#define ARG_TOOLS_H

#include <stdbool.h>
#include <stdint.h> // For fixed-width integer types
#include <stddef.h>  // For size_t


typedef enum {
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
    TYPE_STRING = 's', // For strings (char*)
    TYPE_POINTER = 'p', // For pointers, including arbitrary structs
    TYPE_VOID = 'v',    // For functions with no return value
    TYPE_UNKNOWN = -1 // For unknown types
} ArgType;




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
} ArgInfo;

typedef struct FunctionCallInfo {
    char* library_path;
    char* function_name;
    ArgType return_type;
    size_t return_size;    // Use for pointer return types to indicate how much memory to read
    ArgInfo* args;
    int arg_count;
} FunctionCallInfo;

// Function declarations
char typeToChar(ArgType type);
ArgType charToType(char c);
void freeFunctionCallInfo(FunctionCallInfo* info);
bool isAllDigits(const char* str);
bool isHexFormat(const char* str);
bool endsWith(const char* str, char c);
bool isFloatingPoint(const char* str);
ArgType infer_arg_type(const char* arg);
void* hex_string_to_pointer(const char* hexStr);
void setArgInfoValue(ArgInfo* arg, const void* value);
void convert_arg_value(ArgInfo* arg, const char* argStr);
void log_function_call_info(FunctionCallInfo* info);

#endif /* ARG_TOOLS_H */
