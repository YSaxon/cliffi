#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "types_and_utils.h"
#include "return_formatter.h"


bool isAllDigits(const char* str) {
    return str && *str && strspn(str, "0123456789") == strlen(str);
}

bool isHexFormat(const char* str) {
    return str && strlen(str) > 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X') &&
           strspn(str + 2, "0123456789abcdefABCDEF") == strlen(str) - 2;
}

bool endsWith(const char* str, char c) {
    size_t len = strlen(str);
    return len > 0 && str[len - 1] == c;
}

bool isFloatingPoint(const char* str) {
    // Simple floating-point detection; consider enhancing for full spec compliance
    bool hasDecimal = false;
    bool hasExponent = false;
    bool hasDigit = false;

    for (size_t i = 0; str[i] != '\0'; i++) {
        if (isdigit(str[i])) {
            hasDigit = true;
        } else if (str[i] == '.') {
            if (hasDecimal || hasExponent) {
                return false; // Multiple decimal points or already has exponent
            }
            hasDecimal = true;
        } else if (str[i] == 'e' || str[i] == 'E') {
            if (!hasDigit || hasExponent) {
                return false; // Exponent without digits or multiple exponents
            }
            hasExponent = true;
        } else if (str[i] == '-' || str[i] == '+') {
            if (i != 0 && (str[i - 1] != 'e' && str[i - 1] != 'E')) {
                return false; // Sign in the middle of the string
            }
        } else if (i==strlen(str)-1 && (str[i] == 'd' || str[i] == 'D' || str[i] == 'f' || str[i] == 'F')) { 
                continue; // allow d or D or f or F at the end of the string
        }
        else {
            return false; // Invalid character
        }
    }

    return hasDigit && (hasDecimal || hasExponent);
}

ArgType infer_arg_type(const char* arg) {
    if (!arg || strlen(arg) == 0) return TYPE_UNKNOWN;
    if (arg[0] == '"' || arg[0] == '\'') return TYPE_STRING;
    if (isFloatingPoint(arg)){
            if (endsWith(arg, 'D') || endsWith(arg, 'd')) return TYPE_DOUBLE;
            if (endsWith(arg, 'F') || endsWith(arg, 'f')) return TYPE_FLOAT;
         return TYPE_FLOAT;
         }
    if (isAllDigits(arg)) {
            if (endsWith(arg, 'L') || endsWith(arg, 'l')) return TYPE_LONG;
            if (endsWith(arg, 'U') || endsWith(arg, 'u')) return TYPE_UINT;
            if (endsWith(arg, 'I') || endsWith(arg, 'i')) return TYPE_INT;
        return TYPE_INT;
        }
    if (isHexFormat(arg)) {
        size_t length = strlen(arg);
        if (length <= 4) return TYPE_UCHAR;
        if (length <= 6) return TYPE_USHORT;
        if (length <= 10) return TYPE_UINT;
        return TYPE_ULONG;
    }

    // Add more rules as needed
    return TYPE_STRING; // Default fallback
}

void* hex_string_to_pointer(const char* hexStr) {
    if (!hexStr) return NULL;

    size_t len = strlen(hexStr);
    if (len % 2 != 0) return NULL; // Hex string length must be even

    size_t finalLen = len / 2;
    unsigned char* output = malloc(finalLen);
    if (!output) return NULL;

    for (size_t i = 0, j = 0; i < len; i += 2, j++) {
        int val1 = isdigit(hexStr[i]) ? hexStr[i] - '0' : toupper(hexStr[i]) - 'A' + 10;
        int val2 = isdigit(hexStr[i + 1]) ? hexStr[i + 1] - '0' : toupper(hexStr[i + 1]) - 'A' + 10;

        if (val1 < 0 || val1 > 15 || val2 < 0 || val2 > 15) {
            free(output); // Cleanup on error
            return NULL;
        }

        output[j] = (val1 << 4) + val2;
    }

    return output;
}

void* convert_to_type(ArgType type, const char* argStr) {
    void* result = malloc(typeToSize(type));

    if (result == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        return NULL;
    }

    switch (type) {
        case TYPE_INT: *(int*)result = atoi(argStr); break;
        case TYPE_FLOAT: *(float*)result = atof(argStr); break;
        case TYPE_DOUBLE: *(double*)result = strtod(argStr, NULL); break;
        case TYPE_CHAR: 
            if (isHexFormat(argStr)) {
                *(char*)result = ((char *) hex_string_to_pointer(argStr))[0];
            } else {
                *(char*)result = argStr[0]; 
            }
            break;
        case TYPE_SHORT: *(short*)result = (short)strtol(argStr, NULL, 0); break;
        case TYPE_UCHAR: *(unsigned char*)result = (unsigned char)strtoul(argStr, NULL, 0); break;
        case TYPE_USHORT: *(unsigned short*)result = (unsigned short)strtoul(argStr, NULL, 0); break;
        case TYPE_UINT: *(unsigned int*)result = (unsigned int)strtoul(argStr, NULL, 0); break;
        case TYPE_ULONG: *(unsigned long*)result = strtoul(argStr, NULL, 0); break;
        case TYPE_LONG: *(long*)result = strtol(argStr, NULL, 0); break;
        case TYPE_STRING: *(char**)result = strdup(argStr); break;
        default:
            free(result);
            fprintf(stderr, "Unsupported argument type. Cannot convert value %s.\n", argStr);
            return NULL;
    }
    return result;
}


// Type converter
void convert_arg_value(ArgInfo* arg, const char* argStr) {
    if (arg->is_array){
        handle_array_arginfo_conversion(arg, argStr);
    }
    else{
    void* convertedValue = convert_to_type(arg->type, argStr);
        if (convertedValue == NULL) {
            fprintf(stderr, "Error: Failed to convert argument value %s to type %c\n", argStr, typeToChar(arg->type));
            exit(1);
        }

    ArgType type = arg->type;
    switch (arg->type) {
        case TYPE_INT: arg->value.i_val = *(int*)convertedValue; break;
        case TYPE_FLOAT: arg->value.f_val = *(float*)convertedValue; break;
        case TYPE_DOUBLE: arg->value.d_val = *(double*)convertedValue; break;
        case TYPE_CHAR: arg->value.c_val = *(char*)convertedValue; break;
        case TYPE_SHORT: arg->value.s_val = *(short*)convertedValue; break;
        case TYPE_UCHAR: arg->value.uc_val = *(unsigned char*)convertedValue; break;
        case TYPE_USHORT: arg->value.us_val = *(unsigned short*)convertedValue; break;
        case TYPE_UINT: arg->value.ui_val = *(unsigned int*)convertedValue; break;
        case TYPE_ULONG: arg->value.ul_val = *(unsigned long*)convertedValue; break;
        case TYPE_LONG: arg->value.l_val = *(long*)convertedValue; break;
        case TYPE_STRING: arg->value.str_val = *(char**)convertedValue; break;
        default:
            fprintf(stderr, "Unsupported argument type. Cannot assign value.\n");
            break;
        }
        free(convertedValue);
    }


for (int i = 0; i < arg->pointer_depth; i++) {
    void* temp = malloc(sizeof(arg->value));
    memcpy(temp, &arg->value, sizeof(arg->value));
    arg->value.ptr_val = temp; //TODO we should free it at the end
    // arg->value.ptr_val = &arg->value; this doesn't work because it just ends up pointing to itself
    //don't make the mistake of setting type to POINTER. type is the type of the value being pointed to, not the pointer itself
    }

}

void handle_array_arginfo_conversion(ArgInfo* arg, const char* argStr){


    // three different cases for array argStr
    // 1. single value that is a number, which is the count of the array
    // 2. single value that is a hex string, which is the raw values for the array
    // 3. comma delimitted list of values for the array

    // Step 1: Split string by commas and count substrings
    int count = 1;
    size_t array_size;
    size_t size_of_type = typeToSize(arg->type);
    for (const char* p = argStr; *p; p++) {
        if (*p == ',') count++;
    }

    if (arg->array_size != -1){
        // it has already been set presumably by appending a number to the flag
        if (!(strcmp(argStr, "0") == 0 || strcmp(argStr, "NULL") == 0 || strcmp(argStr, "null") == 0)){
            fprintf(stderr, "Error: Array size already set to %zu, but a size was also at least implicitly specified in the argument after the flag %s\n", arg->array_size, argStr);
            exit(1);
        }
    }

    if (count == 1){
        if (isAllDigits(argStr))
        {
        // consider argstr to be the count for a null array we need to allocate
        array_size = atoi(argStr);
        if (array_size <= 0) {
            fprintf(stderr, "Error: array sizing failed using as count the single value after array flag %s\n", argStr);
            exit(1);
        }
            //allocate a null array of the appropriate type and size
            arg->value.ptr_val = calloc(array_size, size_of_type);
        }
        else if (isHexFormat(argStr))
        {
            //consider argstr to be a hex string containing the raw values for the array (mostly only useful for char arrays)
            int hexstring_bytes = (strlen(argStr) - 2) / 2;
            if (size_of_type == 0) {
                fprintf(stderr, "Error: Unsupported type for array: %c\n with size of 0", typeToChar(arg->type));
                exit(1);
            }
            if (hexstring_bytes % size_of_type != 0) {
                fprintf(stderr, "Error: Hex string bytes length %d is not a multiple of the size of the type %zu\n in hex string being converted to array %s", hexstring_bytes, size_of_type, argStr);
                exit(1);
            }
            array_size = hexstring_bytes / size_of_type;
            arg->value.ptr_val = hex_string_to_pointer(argStr);
        }
        else
        {
            fprintf(stderr, "Error: Unsupported array flag %s. There's no point in having a single element array, just use a pointer\n", argStr);
            // should we actually throw an error, or just let things flow down to the next case?
            exit(1);
        }
    }
    else // we have more than one comma delimitted array value
    {
        array_size = count;
        arg->value.ptr_val = malloc(count * size_of_type);
        char* rest = strdup(argStr);
        for (int i = 0; i < count; i++) {
            char* token = strtok_r(rest, ",", &rest);
            if (token == NULL) {
                fprintf(stderr, "Error: Failed to tokenize array string %s, probably an off by one error\n", argStr);
                exit(1);
            }
            void* convertedValue = convert_to_type(arg->type, token);
            memcpy(arg->value.ptr_val + (i * size_of_type), convertedValue, size_of_type);
            free(convertedValue);
        }
    }
    arg->array_size = array_size;
}


char typeToChar(ArgType type) {
    return type == TYPE_UNKNOWN ? '?' : (char) type;
}

char* typeToString(ArgType type) {
    switch (type) {
        case TYPE_CHAR: return "char";
        case TYPE_SHORT: return "short";
        case TYPE_INT: return "int";
        case TYPE_LONG: return "long";
        case TYPE_UCHAR: return "unsigned char";
        case TYPE_USHORT: return "unsigned short";
        case TYPE_UINT: return "unsigned int";
        case TYPE_ULONG: return "unsigned long";
        case TYPE_FLOAT: return "float";
        case TYPE_DOUBLE: return "double";
        case TYPE_STRING: return "string";
        case TYPE_POINTER: return "pointer";
        case TYPE_VOID: return "void";
        case TYPE_ARRAY: return "array";
        case TYPE_UNKNOWN: return "unknown";
        default: return "other?";
    }
}

size_t typeToSize(ArgType type) {
    switch (type) {
        case TYPE_CHAR: return sizeof(char);
        case TYPE_SHORT: return sizeof(short);
        case TYPE_INT: return sizeof(int);
        case TYPE_LONG: return sizeof(long);
        case TYPE_UCHAR: return sizeof(unsigned char);
        case TYPE_USHORT: return sizeof(unsigned short);
        case TYPE_UINT: return sizeof(unsigned int);
        case TYPE_ULONG: return sizeof(unsigned long);
        case TYPE_FLOAT: return sizeof(float);
        case TYPE_DOUBLE: return sizeof(double);
        case TYPE_STRING: return sizeof(char*);
        case TYPE_POINTER: return sizeof(void*);
        case TYPE_VOID: return 0;
        case TYPE_ARRAY: return sizeof(void*);
        case TYPE_UNKNOWN: return 0;
        default: return 0;
    }
}

char* typeToFormatSpecifier(ArgType type) {
    switch (type) {
        case TYPE_CHAR: return "%c";
        case TYPE_SHORT: return "%hd";
        case TYPE_INT: return "%d";
        case TYPE_LONG: return "%ld";
        case TYPE_UCHAR: return "%u";
        case TYPE_USHORT: return "%hu";
        case TYPE_UINT: return "%u";
        case TYPE_ULONG: return "%lu";
        case TYPE_FLOAT: return "%f";
        case TYPE_DOUBLE: return "%lf";
        case TYPE_STRING: return "%s";
        case TYPE_POINTER: return "%p  (likely mistake)";
        case TYPE_VOID: return "(void)  (likely mistake)";
        default: return "%x  (unsupported type)";
    }
}

ArgType charToType(char c) {
    switch (c) {
        case 'c': return TYPE_CHAR;
        case 'h': return TYPE_SHORT;
        case 'i': return TYPE_INT;
        case 'l': return TYPE_LONG;
        case 'C': return TYPE_UCHAR;
        case 'H': return TYPE_USHORT;
        case 'I': return TYPE_UINT;
        case 'L': return TYPE_ULONG;
        case 'f': return TYPE_FLOAT;
        case 'd': return TYPE_DOUBLE;
        case 's': return TYPE_STRING;
        case 'p': return TYPE_POINTER;
        case 'v': return TYPE_VOID;
        case 'a': return TYPE_ARRAY;
        default: return TYPE_UNKNOWN; // Default or error handling
    }
}

void log_function_call_info(FunctionCallInfo* info){
    //this should all be fprintf(stderr, ...)
    if (!info) {
        printf("No function call info to display.\n");
        return;
    }
    printf("FunctionCallInfo:\n");
    printf("\tLibrary Path: %s\n", info->library_path);
    printf("\tFunction Name: %s\n", info->function_name);
    printf("\tReturn Type: %c\n", typeToChar(info->return_var.type));
    printf("\tArg Count: %d\n", info->arg_count);
    for (int i = 0; i < info->arg_count; i++) {
        printf("Arg %d: %s Type: %c, Value: ", i,info->args[i].explicitType? "Explicit" : "Implicit", typeToChar(info->args[i].type));//, format_buffer);
        format_and_print_arg_value(&info->args[i]);//, format_buffer, buffer_size);
    }
}

void freeArgInfo(ArgInfo* arg){
    void* temp = &arg->value.ptr_val; 

    for (int i = 0; i < arg->pointer_depth; i++) {
        temp = *(void**)temp;
    }

    if (arg->is_array && (arg->type==TYPE_STRING /* || arg->type==TYPE_STRUCT */)){
        for (int i = 0; i < arg->array_size; i++) {
            free((*((char***)temp))[i]);
        }
    }
    if (arg->is_array || arg->type==TYPE_STRING /*|| arg->type==TYPE_STRUCT*/ ){
        free(*((void**)temp));
    }

    //now free them in reverse order
    for (int i = 0; i < arg->pointer_depth; i++) {
        void* parent = &temp;
        free(temp);
        temp = parent;
    }
    //it's possible that the last layer of free will fail since the address is not malloced, if so then just subtract one iteration from the final for loop

}

void freeFunctionCallInfo(FunctionCallInfo* info) {
    if (info) {
        free(info->library_path); // Assuming this was dynamically allocated
        free(info->function_name);
        for (int i = 0; i < info->arg_count; i++) {
            freeArgInfo(&info->args[i]);
        }
        free(info->args);
        free(info);
    }
}

