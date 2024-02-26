#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "types_and_utils.h"


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

// Type converter
void convert_arg_value(ArgInfo* arg, const char* argStr) {
    ArgType type = arg->type;
    switch (type) {
        case TYPE_INT: arg->value.i_val = atoi(argStr); break;
        case TYPE_FLOAT: arg->value.f_val = atof(argStr); break;
        case TYPE_DOUBLE: arg->value.d_val = strtod(argStr, NULL); break;
        case TYPE_CHAR: 
            if (isHexFormat(argStr)) {
                arg->value.c_val = ((char *) hex_string_to_pointer(argStr))[0];
            } else {
                arg->value.c_val = argStr[0]; 
            }
            break;
        case TYPE_SHORT: arg->value.s_val = (short)strtol(argStr, NULL, 0); break;
        case TYPE_UCHAR: arg->value.uc_val = (unsigned char)strtoul(argStr, NULL, 0); break;
        case TYPE_USHORT: arg->value.us_val = (unsigned short)strtoul(argStr, NULL, 0); break;
        case TYPE_UINT: arg->value.ui_val = (unsigned int)strtoul(argStr, NULL, 0); break;
        case TYPE_ULONG: arg->value.ul_val = strtoul(argStr, NULL, 0); break;
        case TYPE_LONG: arg->value.l_val = strtol(argStr, NULL, 0); break;
        case TYPE_STRING: arg->value.str_val = strdup(argStr); break;
        case TYPE_POINTER: 
            if (isHexFormat(argStr)) {
                arg->value.ptr_val = hex_string_to_pointer(argStr);
            } else {
                arg->value.ptr_val = (void*)argStr;
            }
            break;
        // Add conversion logic for other types
        default:
            fprintf(stderr, "Unsupported argument type %c. Cannot convert value %s.\n", typeToChar(arg->type), argStr);
            break;
         break; // Handle as needed
    }
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
        case TYPE_UNKNOWN: return "unknown";
        default: return "other?";
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
        default: return TYPE_VOID; // Default or error handling
    }
}

 void setArgInfoValue(ArgInfo* arg, const void* value){
    switch(arg->type){
        case TYPE_CHAR:
            arg->value.c_val = *(char*)value;
            break;
        case TYPE_SHORT:
            arg->value.s_val = *(short*)value;
            break;
        case TYPE_INT:
            arg->value.i_val = *(int*)value;
            break;
        case TYPE_LONG:
            arg->value.l_val = *(long*)value;
            break;
        case TYPE_UCHAR:
            arg->value.uc_val = *(unsigned char*)value;
            break;
        case TYPE_USHORT:
            arg->value.us_val = *(unsigned short*)value;
            break;
        case TYPE_UINT:
            arg->value.ui_val = *(unsigned int*)value;
            break;
        case TYPE_ULONG:
            arg->value.ul_val = *(unsigned long*)value;
            break;
        case TYPE_FLOAT:
            arg->value.f_val = *(float*)value;
            break;
        case TYPE_DOUBLE:
            arg->value.d_val = *(double*)value;
            break;
        case TYPE_STRING:
            arg->value.str_val = strdup(*(char**)value);
            break;
        case TYPE_POINTER:
            arg->value.ptr_val = *(void**)value;
            break;
        case TYPE_VOID:
            if (value) {
                fprintf(stderr, "Error: Cannot set value for void type\n");
                exit(1);
            }
            // fprintf(stderr, "Error: Cannot set value for void type\n");
            // exit(1);
        default:
            fprintf(stderr, "Error: Unsupported type\n");
            exit(1);
    }
}

void log_function_call_info(FunctionCallInfo* info){
    //this should all be fprintf(stderr, ...)
    if (!info) {
        printf("No function call info to display.\n");
        return;
    }
    printf("FunctionCallInfo:\n");
    printf("Library Path: %s\n", info->library_path);
    printf("Function Name: %s\n", info->function_name);
    printf("Return Type: %c\n", typeToChar(info->return_type));
    printf("Arg Count: %d\n", info->arg_count);
    for (int i = 0; i < info->arg_count; i++) {
        printf("Arg %d: %s Type: %c, Value: ", i,info->args[i].explicitType? "Explicit" : "Implicit", typeToChar(info->args[i].type));
        switch (info->args[i].type) {
            case TYPE_CHAR:
                printf("%c\n", info->args[i].value.c_val);
                break;
            case TYPE_SHORT:
                printf("%hd\n", info->args[i].value.s_val);
                break;
            case TYPE_INT:
                printf("%d\n", info->args[i].value.i_val);
                break;
            case TYPE_LONG:
                printf("%ld\n", info->args[i].value.l_val);
                break;
            case TYPE_UCHAR:
                printf("%u\n", (unsigned)info->args[i].value.uc_val);
                break;
            case TYPE_USHORT:
                printf("%hu\n", info->args[i].value.us_val);
                break;
            case TYPE_UINT:
                printf("%u\n", info->args[i].value.ui_val);
                break;
            case TYPE_ULONG:
                printf("%lu\n", info->args[i].value.ul_val);
                break;
            case TYPE_FLOAT:
                printf("%f\n", info->args[i].value.f_val);
                break;
            case TYPE_DOUBLE:
                printf("%lf\n", info->args[i].value.d_val);
                break;
            case TYPE_STRING:
                printf("%s\n", info->args[i].value.str_val);
                break;
            case TYPE_POINTER:
                printf("%p\n", info->args[i].value.ptr_val);
                break;
            case TYPE_VOID:
                printf("(void)\n");
                break;
            default:
                printf("Unsupported type\n");
                break;
        }
    }
}

void freeFunctionCallInfo(FunctionCallInfo* info) {
    if (info) {
        free(info->library_path); // Assuming this was dynamically allocated
        free(info->function_name);
            free(info->args);
        free(info);
    }
}
