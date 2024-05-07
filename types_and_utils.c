#include "types_and_utils.h"
#include "invoke_handler.h"
#include "main.h"
#include "parse_address.h"
#include "return_formatter.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* trim_whitespace(char* str)
// https://stackoverflow.com/a/122974
{
    size_t len = 0;
    char* frontp = str;
    char* endp = NULL;

    if (str == NULL) {
        return NULL;
    }
    if (str[0] == '\0') {
        return str;
    }

    len = strlen(str);
    endp = str + len;

    /* Move the front and back pointers to address the first non-whitespace
     * characters from each end.
     */
    while (isspace((unsigned char)*frontp)) {
        ++frontp;
    }
    if (endp != frontp) {
        while (isspace((unsigned char)*(--endp)) && endp != frontp) {
        }
    }

    if (frontp != str && endp == frontp) {
        // Empty string
        *(isspace((unsigned char)*endp) ? str : (endp + 1)) = '\0';
    } else if (str + len - 1 != endp)
        *(endp + 1) = '\0';

    /* Shift the string so that it starts at str so that if it's dynamically
     * allocated, we can still free it on the returned pointer.  Note the reuse
     * of endp to mean the front of the string buffer now.
     */
    endp = str;
    if (frontp != str) {
        while (*frontp) {
            *endp++ = *frontp++;
        }
        *endp = '\0';
    }

    return str;
}

void* makePointerLevel(void* value, int pointer_depth) {
    for (int i = 0; i < pointer_depth; i++) {
        void* temp = malloc(sizeof(void*)); // Allocate memory for each level of indirection
        *(void**)temp = value;              // Store the pointer to the previous level
        value = temp;
    }
    return value;
}

void* dereferencePointerLevels(void* value, int pointer_depth) {
    for (int i = 0; i < pointer_depth; i++) {
        value = *(void**)value;
    }
    return value;
}

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
        } else if (i == strlen(str) - 1 && (str[i] == 'd' || str[i] == 'D' || str[i] == 'f' || str[i] == 'F')) {
            continue; // allow d or D or f or F at the end of the string
        } else {
            return false; // Invalid character
        }
    }

    return hasDigit && (hasDecimal || hasExponent);
}

ArgType infer_arg_type_single(const char* argval) {
    if (!argval) return TYPE_UNKNOWN;
    // if (argval[0] == '"' || argval[0] == '\'' || strlen(argval) == 0) return TYPE_STRING; // if it starts with a quote or is empty, it's probably a string
    // if (strchr(argval, ' ') != NULL) return TYPE_STRING; // if there is a space in the value, it's probably a string

    bool is_negative = argval[0] == '-';
    const char* without_negative_sign = is_negative ? argval + 1 : argval;
    if (isFloatingPoint(without_negative_sign)) {
        if (endsWith(argval, 'D') || endsWith(argval, 'd')) return TYPE_DOUBLE;
        if (endsWith(argval, 'F') || endsWith(argval, 'f')) return TYPE_FLOAT;
        return TYPE_DOUBLE; // probably the most common
    }
    if (isAllDigits(without_negative_sign)) {
        if (endsWith(argval, 'L') || endsWith(argval, 'l')) return TYPE_LONG;
        if (endsWith(argval, 'U') || endsWith(argval, 'u')) return TYPE_UINT;
        if (endsWith(argval, 'I') || endsWith(argval, 'i')) return TYPE_INT;
        // test if value is too big for int
        long long int test = strtoll(argval, NULL, 0);
        if (errno == ERANGE) {
            fprintf(stderr, "Error: Value %s is out of range even for long long\n", argval);
            exit_or_restart(1);
        }
        if (test > ULONG_MAX) {
            // todo implement longlong?
            fprintf(stderr, "Error: Value %s is too large to fit into an unsigned long, and we haven't implemented longlong yet\n", argval);
            exit_or_restart(1);
        } else if (test > LONG_MAX) {
            return TYPE_ULONG; // no alternative so long as we don't have longlong
        } else if (test > UINT_MAX) {
            return TYPE_LONG; // it COULD still be ULONG but we'll assume it's just long if it fits in long
        } else if (test > INT_MAX) {
            return TYPE_UINT; // should we infer a long instead? What's more common?
        } else if (test < INT_MIN) {
            return TYPE_LONG; // no alternative so long as we don't have longlong
        } else {
            return TYPE_INT; // if it fits in an int we'll just use that since that's most common
        }
    }
    if (isHexFormat(without_negative_sign)) {
        size_t length = strlen(without_negative_sign);
        if (without_negative_sign[0] == '0' && (without_negative_sign[1] == 'x' || without_negative_sign[1] == 'X')) length -= 2; // Skip 0x (if present)
        length /= 2;                                                                                                              // Two hex characters per byte
        if (length <= sizeof(char)) return is_negative ? TYPE_CHAR : TYPE_UCHAR;
        if (length <= sizeof(short)) return is_negative ? TYPE_SHORT : TYPE_USHORT;
        if (!is_negative && length <= sizeof(void*)) return TYPE_VOIDPOINTER; // voidpointer is more common for hex so if it fits we'll use that
        if (length <= sizeof(int)) return is_negative ? TYPE_INT : TYPE_UINT;
        if (length <= sizeof(long))
            return is_negative ? TYPE_LONG : TYPE_ULONG;
        else {
            fprintf(stderr, "Error: Hex string %s is %zu bytes, which is too long to fit into a single type. If you meant to specify an array or a string, flag it as such.\n", argval, length);
            exit_or_restart(1);
        }
    }

    if (strlen(argval) == 1) return TYPE_CHAR;
    return TYPE_STRING; // Default fallback
}

void infer_arg_type_from_value(ArgInfo* arg, const char* argval) {
    arg->explicitType = false;
    arg->pointer_depth = 0;

    // infer array type by presence of commas
    // check that for every substring, infer_arg_type returns the same? or just use the first type?
    if (strchr(argval, ',') != NULL) {
        char* rest = strdup(argval);
        char* rest_copy_to_free = rest;
        char* token = strtok_r(rest, ",", &rest);
        ArgType first_type = infer_arg_type_single(token);
        while (token != NULL) {
            ArgType next_type = infer_arg_type_single(token);
            if (next_type != first_type) {
                fprintf(stderr, "Warning: In argstr %s, multiple types found in comma delimitted list. We'll use the first type %s\n", argval, typeToString(first_type));
                break;
            }
            token = strtok_r(rest, ",", &rest);
        }
        arg->type = first_type;
        arg->is_array = ARRAY_STATIC_SIZE_UNSET; // we'll set the size later anyway, just as if it were specified as an array, but with size unspecified
        free(rest_copy_to_free);
    } else {
        arg->type = infer_arg_type_single(argval);
        arg->is_array = NOT_ARRAY;
    }
}

void* hex_string_to_bytes(const char* hexStr) {
    if (!hexStr) return NULL;
    if (hexStr[0] == '0' && (hexStr[1] == 'x' || hexStr[1] == 'X')) hexStr += 2; // Skip 0x (if present)

    size_t len = strlen(hexStr);
    if (len == 0 || len % 2 != 0) return NULL; // Hex string length must be non-zero and even

    size_t finalLen = len / 2;
    unsigned char* output = malloc(finalLen);
    if (!output) return NULL;

    for (size_t i = 0, j = 0; i < len; i += 2, j++) {
        int val1 = isxdigit(hexStr[i]) ? (isdigit(hexStr[i]) ? hexStr[i] - '0' : toupper(hexStr[i]) - 'A' + 10) : -1;
        int val2 = isxdigit(hexStr[i + 1]) ? (isdigit(hexStr[i + 1]) ? hexStr[i + 1] - '0' : toupper(hexStr[i + 1]) - 'A' + 10) : -1;

        if (val1 < 0 || val2 < 0) {
            // free(output); // Cleanup on error
            fprintf(stderr, "Error: Invalid hex character in string: %s\n", hexStr);
            exit_or_restart(1);
        }

        output[j] = (unsigned char)((val1 << 4) + val2);
    }

    return output;
}

void* convert_to_type(ArgType type, const char* argStr) {
    void* result = malloc(typeToSize(type, 0));

    if (result == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        return NULL;
    }

    switch (type) {
    case TYPE_INT:
        *(int*)result = (int)strtol(argStr, NULL, 0);
        break;
    case TYPE_FLOAT:
        *(float*)result = strtof(argStr, NULL);
        break;
    case TYPE_DOUBLE:
        *(double*)result = strtod(argStr, NULL);
        break;
    case TYPE_CHAR:
        if (isHexFormat(argStr)) {
            *(char*)result = ((char*)hex_string_to_bytes(argStr))[0];
        } else {
            *(char*)result = argStr[0];
        }
        break;
    case TYPE_SHORT:
        *(short*)result = (short)strtol(argStr, NULL, 0);
        break;
    case TYPE_UCHAR:
        *(unsigned char*)result = (unsigned char)strtoul(argStr, NULL, 0);
        break;
    case TYPE_USHORT:
        *(unsigned short*)result = (unsigned short)strtoul(argStr, NULL, 0);
        break;
    case TYPE_UINT:
        *(unsigned int*)result = (unsigned int)strtoul(argStr, NULL, 0);
        break;
    case TYPE_ULONG:
        *(unsigned long*)result = strtoul(argStr, NULL, 0);
        break;
    case TYPE_LONG:
        *(long*)result = strtol(argStr, NULL, 0);
        break;
    case TYPE_VOIDPOINTER:
        *(void**)result = getAddressFromAddressStringOrNameOfCoercableVariable(argStr);
        break;
    case TYPE_STRING:
        *(char**)result = strdup(argStr);
        break;
    default:
        free(result);
        fprintf(stderr, "Unsupported argument type. Cannot convert value %s.\n", argStr);
        return NULL;
    }
    return result;
}

void set_arg_value_nullish(ArgInfo* arg){
    if (arg->is_array == ARRAY_STATIC_SIZE) { // in that case we mean a null array. (ARRAY_SIZE_AT_ARGNUM is handled in second_pass_arginfo_ptr)
        void* array_raw = calloc(arg->static_or_implied_size, typeToSize(arg->type, arg->array_value_pointer_depth));
        arg->value->ptr_val = makePointerLevel(array_raw, arg->pointer_depth);

    } else if (arg->type == TYPE_STRUCT){
        fprintf(stderr, "Setting struct types to NULL should not be getting handled by this function. Please report this.");
        exit_or_restart(1);
    } else { // non pointer set to null is just setting the value to 0
        memset(arg->value, 0, typeToSize(arg->type, 0));
    }

}

void handle_array_arginfo_conversion(ArgInfo* arg, const char* argStr) {

    // multiple scenarios to deal with:
    // sizing:
    // static size was set by appending a number to the flag
    // size was set to argnum by appending a t to the flag
    // size was not set, and we need to parse the string to determine the size
    // value:
    // value was set to NULL, and we need to allocate the array
    // value was set to a hex string, and we need to convert it to the array
    // value was set to a comma delimitted list of values, and we need to parse it into the array
    // todo: value was set to a string, and we need to parse it into the array (if it's a char or uchar array)

    // we need to deal with each combination of these scenarios
    // essentially the rule should be that if the value is nonnull and therefore implies a size:
    // if no explicit size was given has not already been set then we should set that as size and allocate and copy that exact array of values
    // if an explicit size has already been set then we should check that the size is consistent with the value
    // if the explicit size is smaller than the actual value implies then emit a warning and truncate the value
    // if the explicit size is greater than the actual value implies then emit a warning and fill the rest of the array with 0s
    // if the size was given as an argnum then we should just assume that the value is the correct size and allocate and copy that exact array of values
    //(for the moment just emit a warning that we aren't checking that the size is consistent with the value, but we should add that check later)
    // if the value is null (or the size is not implied by the value) then we should do the following:
    // if an explicit size was given then allocate an empty array of that size
    // if the size was given as an argnum then we'll need to allocate an empty array of that size sometime later, after we've parsed the rest of the args
    //(for the moment just emit a warning that we aren't allocating arrays for null values that are sized by argnum, but we should add that later)
    // if the size is not given statically or by argnum then we should emit an error that we cannot initialize a null array with no sizing info and if they want a null pointer they should use the pointer flag instead

    // if (arg->is_array==ARRAY_STATIC_SIZE) {

    if ((strcmp(argStr, "0") == 0 || strcmp(argStr, "NULL") == 0 || strcmp(argStr, "null") == 0)) {
        fprintf(stderr, "Setting an array to NULL this way is deprecated. Please use the newer more flexible syntax, replacing the dash in the type with an n, like nai4 for a null array of 4 ints");
        if (arg->is_array == ARRAY_STATIC_SIZE) {
            arg->value->ptr_val = calloc(arg->static_or_implied_size, typeToSize(arg->type, arg->array_value_pointer_depth));
            return;
        } else if (arg->is_array == ARRAY_SIZE_AT_ARGNUM) {
            // fprintf(stderr, "Warning: We have not yet implemented initializing null arrays of size pointed to by another argument, so this will be a null pointer for now\n");
            // we've now implemented this in second_pass_arginfo_ptr_sized_null_array_initialization, so we can just return
            arg->value->ptr_val = NULL;
            return;
        } else {
            fprintf(stderr, "Error: Argstr %s is interpreted as NULL. We cannot initialize a null array with no sizing info. If you WANT a null pointer you should use the pointer flag instead\n", argStr);
            exit_or_restart(1);
        }
    }

    // three different cases for array argStr
    // 1. single value that is a number, which is the count of the array <-- removed on account of being duplicative with the unspace syntax we use for return values
    // 2. single value that is a hex string, which is the raw values for the array
    // 3. comma delimitted list of values for the array

    // Step 1: Split string by commas and count substrings
    size_t array_size_implicit;
    void* array_values;
    size_t size_of_type = typeToSize(arg->type, arg->array_value_pointer_depth);

    int count = 1;
    for (const char* p = argStr; *p; p++) {
        if (*p == ',') count++;
    }

    if (count == 1) {
        if (isHexFormat(argStr)) {
            if (arg->array_value_pointer_depth > 0) {
                fprintf(stderr, "Error: You can't use the hex array initialization method with an array of pointer types");
                exit_or_restart(1);
            }
            // consider argstr to be a hex string containing the raw values for the array (mostly only useful for char arrays)
            int hexstring_bytes = (strlen(argStr) - 2) / 2;
            if (size_of_type == 0) {
                fprintf(stderr, "Error: Unsupported type for array: %c\n with size of 0", typeToChar(arg->type));
                exit_or_restart(1);
            }
            if (hexstring_bytes % size_of_type != 0) {
                fprintf(stderr, "Error: Hex string bytes length %d is not a multiple of the size of the type %zu\n in hex string being converted to array %s", hexstring_bytes, size_of_type, argStr);
                exit_or_restart(1);
            }
            array_size_implicit = hexstring_bytes / size_of_type;
            array_values = hex_string_to_bytes(argStr);
        } else {
            fprintf(stderr, "Warning: In argstr %s, no valid hex value found, no static or dynamic size found, and no commas found to delimit values. We'll attempt to parse it as a single element array, but that's probably not what you intended.\n", argStr);
            goto parseascommadelimited;
        }
    } else
    parseascommadelimited: { // if we didn't already parse it as a hex string, then we'll parse it as a comma delimitted list of values
        array_size_implicit = count;
        array_values = calloc(count, size_of_type);
        char* rest = strdup(argStr);
        for (int i = 0; i < count; i++) {
            char* token = strtok_r(rest, ",", &rest);
            if (token == NULL) {
                fprintf(stderr, "Error: Failed to tokenize array string %s, probably an off by one error\n", argStr);
                exit_or_restart(1);
            }
            void* convertedValue = convert_to_type(arg->type, token);
            convertedValue = makePointerLevel(convertedValue, arg->array_value_pointer_depth);
            memcpy(array_values + (i * size_of_type), convertedValue, size_of_type);
            free(convertedValue);
        }
    }
        if (arg->is_array == ARRAY_STATIC_SIZE_UNSET) {
            arg->is_array = ARRAY_STATIC_SIZE;
            arg->static_or_implied_size = array_size_implicit;
            arg->value->ptr_val = array_values;
        } else if (arg->is_array == ARRAY_STATIC_SIZE) {
            size_t explicit_size = arg->static_or_implied_size;
            size_t implicit_size = array_size_implicit;
            if (explicit_size < implicit_size) {
                fprintf(stderr, "Warning: Array was specified to have size %zu, but the value implies a size of %zu. Setting array size to the explicit size (values may be truncated)\n", explicit_size, implicit_size);
                arg->value->ptr_val = array_values;
                arg->static_or_implied_size = explicit_size;
            } else if (explicit_size > implicit_size) {
                fprintf(stderr, "Warning: Array was specified to have size %zu, but the value implies a size of %zu. Filling the rest of the array with null bytes\n", explicit_size, implicit_size);
                arg->value->ptr_val = calloc(explicit_size, size_of_type);
                memcpy(arg->value->ptr_val, array_values, implicit_size * size_of_type);
                free(array_values);
            } else {
                arg->value->ptr_val = array_values;
                arg->static_or_implied_size = explicit_size;
            }
        } else if (arg->is_array == ARRAY_SIZE_AT_ARGNUM) {
            // fprintf(stderr, "Warning: we are initializing an argnum size_t sized array with the implicit size %zu implied by the value it is being initialized with. On the second pass of the parser, we will reallocate the array, but this may lead to truncation or \n");
            arg->static_or_implied_size = array_size_implicit;
            arg->value->ptr_val = array_values;
            // arg->static_or_implied_size = array_size_implicit; <-- DONT DO THAT, IT WILL OVERWRITE THE ARGNUM!
            // alternatively we could add a field to the arginfo to store the implicit size
        } else {
            fprintf(stderr, "Error: Unsupported array size mode %d\n", arg->is_array);
            exit_or_restart(1);
        }
}

// Type converter
void convert_arg_value(ArgInfo* arg, const char* argStr) {
    if (arg->is_array) {
        handle_array_arginfo_conversion(arg, argStr);
    } else {
        void* convertedValue = convert_to_type(arg->type, argStr);
        if (convertedValue == NULL) {
            fprintf(stderr, "Error: Failed to convert argument value %s to type %c\n", argStr, typeToChar(arg->type));
            exit_or_restart(1);
        }

        switch (arg->type) {
        case TYPE_INT:
            arg->value->i_val = *(int*)convertedValue;
            break;
        case TYPE_FLOAT:
            arg->value->f_val = *(float*)convertedValue;
            break;
        case TYPE_DOUBLE:
            arg->value->d_val = *(double*)convertedValue;
            break;
        case TYPE_CHAR:
            arg->value->c_val = *(char*)convertedValue;
            break;
        case TYPE_SHORT:
            arg->value->s_val = *(short*)convertedValue;
            break;
        case TYPE_UCHAR:
            arg->value->uc_val = *(unsigned char*)convertedValue;
            break;
        case TYPE_USHORT:
            arg->value->us_val = *(unsigned short*)convertedValue;
            break;
        case TYPE_UINT:
            arg->value->ui_val = *(unsigned int*)convertedValue;
            break;
        case TYPE_ULONG:
            arg->value->ul_val = *(unsigned long*)convertedValue;
            break;
        case TYPE_LONG:
            arg->value->l_val = *(long*)convertedValue;
            break;
        case TYPE_VOIDPOINTER:
            arg->value->ptr_val = *(void**)convertedValue;
            break;
        case TYPE_STRING:
            arg->value->str_val = *(char**)convertedValue;
            break;
        default:
            fprintf(stderr, "Unsupported argument type. Cannot assign value.\n");
            break;
        }
        free(convertedValue);
    }

    for (int i = 0; i < arg->pointer_depth; i++) {
        void* temp = malloc(sizeof(void*)); // meaning size of a pointer
        memcpy(temp, arg->value, sizeof(void*));
        arg->value->ptr_val = temp; // TODO we should free it at the end
        // arg->value.ptr_val = &arg->value; this doesn't work because it just ends up pointing to itself
        // don't make the mistake of setting type to POINTER. type is the type of the value being pointed to, not the pointer itself
    }
}

void second_pass_arginfo_ptr_sized_null_array_initialization_inner(ArgInfo* arg) {
    // first we have to traverse the pointer_depths to get to the actual array
    void* value = arg->value->ptr_val;
    void* parent = arg->value;
    for (int j = 0; j < arg->pointer_depth; j++) {
        parent = value;
        value = *(void**)value;
    }
    if (value == NULL) {
        size_t size = get_size_for_arginfo_sized_array(arg);
        *(void**)parent = calloc(size, typeToSize(arg->type, arg->array_value_pointer_depth));
    } else {
        size_t implied_or_explicit_size = arg->static_or_implied_size;
        size_t size_from_sizet_arg = get_size_for_arginfo_sized_array(arg);

        if (size_from_sizet_arg < implied_or_explicit_size) {
            fprintf(stderr, "Warning: Array was specified by its size_t arg to have size %zu, but the value implies a greater size of %zu. Setting array size to the explicit size (values may be truncated)\n", size_from_sizet_arg, implied_or_explicit_size);
        } else if (size_from_sizet_arg > implied_or_explicit_size) {
            *(void**)parent = calloc(size_from_sizet_arg, typeToSize(arg->type, arg->array_value_pointer_depth));
            memcpy(*(void**)parent, value, implied_or_explicit_size * typeToSize(arg->type, arg->array_value_pointer_depth));
            // should we free the old array? it seems to cause errors if we do
        }
    }
}

void second_pass_arginfo_ptr_sized_null_array_initialization(ArgInfoContainer* info) {
    for (int i = 0; i < info->arg_count; i++) {
        if (info->args[i]->is_array == ARRAY_SIZE_AT_ARGINFO_PTR) {
            second_pass_arginfo_ptr_sized_null_array_initialization_inner(info->args[i]);
        }
    }
    if (info->return_var->is_array == ARRAY_SIZE_AT_ARGINFO_PTR) {
        second_pass_arginfo_ptr_sized_null_array_initialization_inner(info->return_var);
    }
}

char typeToChar(ArgType type) {
    return type == TYPE_UNKNOWN ? '?' : (char)type;
}

char* typeToString(ArgType type) {
    switch (type) {
    case TYPE_CHAR:
        return "char";
    case TYPE_SHORT:
        return "short";
    case TYPE_INT:
        return "int";
    case TYPE_LONG:
        return "long";
    case TYPE_UCHAR:
        return "uchar";
    case TYPE_USHORT:
        return "ushort";
    case TYPE_UINT:
        return "uint";
    case TYPE_ULONG:
        return "ulong";
    case TYPE_FLOAT:
        return "float";
    case TYPE_DOUBLE:
        return "double";
    case TYPE_STRING:
        return "cstring";
    case TYPE_POINTER:
        return "pointer";
    case TYPE_VOIDPOINTER:
        return "(void*)";
    case TYPE_VOID:
        return "void";
    case TYPE_ARRAY:
        return "array";
    case TYPE_UNKNOWN:
        return "unknown";
    case TYPE_STRUCT:
        return "struct";
    default:
        return "other?";
    }
}

size_t typeToSize(ArgType type, int array_value_pointer_depth) {
    if (array_value_pointer_depth > 0) return sizeof(void*);
    switch (type) {
    case TYPE_CHAR:
        return sizeof(char);
    case TYPE_SHORT:
        return sizeof(short);
    case TYPE_INT:
        return sizeof(int);
    case TYPE_LONG:
        return sizeof(long);
    case TYPE_UCHAR:
        return sizeof(unsigned char);
    case TYPE_USHORT:
        return sizeof(unsigned short);
    case TYPE_UINT:
        return sizeof(unsigned int);
    case TYPE_ULONG:
        return sizeof(unsigned long);
    case TYPE_FLOAT:
        return sizeof(float);
    case TYPE_DOUBLE:
        return sizeof(double);
    case TYPE_STRING:
        return sizeof(char*);
    case TYPE_POINTER:
        return sizeof(void*);
    case TYPE_VOIDPOINTER:
        return sizeof(void*);
    case TYPE_VOID:
        return 0;
    case TYPE_ARRAY:
        return sizeof(void*);
    // case TYPE_UNKNOWN: return 0;
    default:
        fprintf(stderr, "Error: Unsupported type %c\n", typeToChar(type));
        exit_or_restart(1);
        fprintf(stderr, "Error: we should never reach here");
        exit(1);
    }
}

char* typeToFormatSpecifier(ArgType type) {
    switch (type) {
    case TYPE_CHAR:
        return "%c";
    case TYPE_SHORT:
        return "%hd";
    case TYPE_INT:
        return "%d";
    case TYPE_LONG:
        return "%ld";
    case TYPE_UCHAR:
        return "%u";
    case TYPE_USHORT:
        return "%hu";
    case TYPE_UINT:
        return "%u";
    case TYPE_ULONG:
        return "%lu";
    case TYPE_FLOAT:
        return "%f";
    case TYPE_DOUBLE:
        return "%lf";
    case TYPE_STRING:
        return "%s";
    case TYPE_POINTER:
        return "%p  (likely mistake)";
    case TYPE_VOIDPOINTER:
        return "%p";
    case TYPE_VOID:
        return "(void)  (likely mistake)";
    default:
        return "%x  (unsupported type)";
    }
}

ArgType charToType(char c) {
    switch (c) {
    case 'c':
        return TYPE_CHAR;
    case 'h':
        return TYPE_SHORT;
    case 'i':
        return TYPE_INT;
    case 'l':
        return TYPE_LONG;
    case 'C':
        return TYPE_UCHAR;
    case 'H':
        return TYPE_USHORT;
    case 'I':
        return TYPE_UINT;
    case 'L':
        return TYPE_ULONG;
    case 'f':
        return TYPE_FLOAT;
    case 'd':
        return TYPE_DOUBLE;
    case 's':
        return TYPE_STRING;
    case 'p':
        return TYPE_POINTER;
    case 'P':
        return TYPE_VOIDPOINTER;
    case 'v':
        return TYPE_VOID;
    case 'a':
        return TYPE_ARRAY;
    case 'S':
        return TYPE_STRUCT;
    default:
        return TYPE_UNKNOWN; // Default or error handling
    }
}

void convert_argnum_sized_array_to_arginfo_ptr(ArgInfo* arg, ArgInfoContainer* info) {
    if (arg->is_array == ARRAY_SIZE_AT_ARGNUM) {
        if (arg->array_sizet_arg.argnum_of_size_t_to_be_replaced > info->arg_count) {
            fprintf(stderr, "Error: array was specified to have its size_t be at argnum %d, but there are only %d args\n", arg->array_sizet_arg.argnum_of_size_t_to_be_replaced, info->arg_count);
            exit_or_restart(1);
        } else if (arg->array_sizet_arg.argnum_of_size_t_to_be_replaced < 0) {
            fprintf(stderr, "Error: array was specified to have its size_t be at argnum %d, but argnums must be positive\n", arg->array_sizet_arg.argnum_of_size_t_to_be_replaced);
            exit_or_restart(1);
        } else {
            arg->is_array = ARRAY_SIZE_AT_ARGINFO_PTR; // TODO maybe we should replace 0 with R for return value?
            if (arg->array_sizet_arg.argnum_of_size_t_to_be_replaced == 0)
                arg->array_sizet_arg.arginfo_of_size_t = (ArgInfo*)info->return_var; // 0 is the return value
            else
                arg->array_sizet_arg.arginfo_of_size_t = info->args[arg->array_sizet_arg.argnum_of_size_t_to_be_replaced - 1]; // -1 because the argnums are 1 indexed
        }
    }
}

void convert_all_arrays_to_arginfo_ptr_sized_after_parsing(ArgInfoContainer* info) {
    for (int i = 0; i < info->arg_count; i++) {
        if (info->args[i]->is_array == ARRAY_SIZE_AT_ARGNUM) {
            convert_argnum_sized_array_to_arginfo_ptr(info->args[i], info);
        }
    }
    if (info->return_var->is_array == ARRAY_SIZE_AT_ARGNUM) { // will always be false if the return value is NULL
        convert_argnum_sized_array_to_arginfo_ptr(info->return_var, info);
    }
}

size_t get_size_for_arginfo_sized_array(const ArgInfo* arg) {
    void* size_t_param_val; // can't declare it in the middle of the switch case, but not assigning here to avoid UB when it's not used
    switch (arg->is_array) {
    case ARRAY_SIZE_AT_ARGINFO_PTR:
        size_t_param_val = &arg->array_sizet_arg.arginfo_of_size_t->value;
        for (int i = 0; i < arg->array_sizet_arg.arginfo_of_size_t->pointer_depth; i++) {
            if (!size_t_param_val) return 0;
            size_t_param_val = *(void**)size_t_param_val;
        }
#if defined(__s390x__)
        return (size_t) * *(unsigned long**)size_t_param_val;
#else
        switch (arg->array_sizet_arg.arginfo_of_size_t->type) {
        case TYPE_SHORT:
            return (size_t) * *(short**)size_t_param_val;
        case TYPE_INT:
            return (size_t) * *(int**)size_t_param_val;
        case TYPE_LONG:
            return (size_t) * *(long**)size_t_param_val;
        case TYPE_UCHAR:
            return (size_t) * *(unsigned char**)size_t_param_val;
        case TYPE_USHORT:
            return (size_t) * *(unsigned short**)size_t_param_val;
        case TYPE_UINT:
            return (size_t) * *(unsigned int**)size_t_param_val;
        case TYPE_ULONG:
            return (size_t) * *(unsigned long**)size_t_param_val;
        default:
            fprintf(stderr, "Error: array was specified to have its size_t be another argument, but the arg at that position is not a numeric type\n");
            exit_or_restart(1);
        }
#endif
    case ARRAY_STATIC_SIZE:
        // fprintf(stderr,"Warning: getSizeForSizeTArray was called on an array with static size\n");
        return arg->static_or_implied_size;
    case ARRAY_STATIC_SIZE_UNSET:
        fprintf(stderr, "Error: getSizeForSizeTArray was called on an array on static_unset mode\n");
        exit_or_restart(1);
    case ARRAY_SIZE_AT_ARGNUM:
        fprintf(stderr, "Error: getSizeForSizeTArray was called on an arginfo with is_array ARRAY_SIZE_AT_ARGNUM. Call convert_all_arrays_to_arginfo_ptr_sized_after_parsing() first.\n");
        exit_or_restart(1);
    case NOT_ARRAY:
        fprintf(stderr, "Error: getSizeForSizeTArray was called on an arginfo with is_array NOT_ARRAY\n");
        exit_or_restart(1);
    default:
        fprintf(stderr, "Error: getSizeForSizeTArray was called on an arginfo with unsupported is_array mode %d\n", arg->is_array);
        exit_or_restart(1);
    }
    fprintf(stderr, "Error: getSizeForSizeTArray fell through to the end of the function\n");
    exit(1);
}

void log_function_call_info(FunctionCallInfo* info) {
    // this should all be fprintf(stderr, ...)
    if (!info) {
        printf("No function call info to display.\n");
        return;
    }
    printf("FunctionCallInfo:\n");
    printf("\tLibrary Path: %s\n", info->library_path);
    printf("\tFunction Name: %s\n", info->function_name);
    printf("\tReturn type: ");
    format_and_print_arg_type(info->info.return_var);
    printf("\n");
    printf("\tArg Count: %d\n", info->info.arg_count);
    for (int i = 0; i < info->info.arg_count; i++) {
        if (info->info.vararg_start == i) printf("\tVarargs start here\n");
        printf("\tArg %d: %s ", i, info->info.args[i]->explicitType ? "(explicit)" : "(inferred)");
        format_and_print_arg_type(info->info.args[i]); //, format_buffer, buffer_size);
        printf(" = ");
        format_and_print_arg_value(info->info.args[i]); //, format_buffer, buffer_size);
        printf("\n");
    }

    // print as function signature
    format_and_print_arg_type(info->info.return_var);
    printf(" %s(", info->function_name);
    for (int i = 0; i < info->info.arg_count; i++) {
        format_and_print_arg_type(info->info.args[i]);
        printf(" ");
        if (info->info.args[i]->is_array) {
            printf("{...}");
        } else {
            format_and_print_arg_value(info->info.args[i]);
        }
        if (i < info->info.arg_count - 1) printf(", ");
    }
    printf(")\n");
}


void* dynamicCast(void* ptr, ArgType from, ArgType to) {
    void* result = NULL;
    if (from == to) {
        // don't return the same pointer, because it might be freed later
        result = malloc(typeToSize(to, 0));
        memcpy(result, ptr, typeToSize(to, 0));
    }

    void* intermediate = NULL;

    // Allocate memory for the intermediate type
    switch (from) {
        case TYPE_CHAR:
        case TYPE_SHORT:
        case TYPE_INT:
        case TYPE_LONG:
        case TYPE_UCHAR:
        case TYPE_USHORT:
        case TYPE_UINT:
        case TYPE_ULONG:
        case TYPE_POINTER:
        case TYPE_VOIDPOINTER:
        case TYPE_STRING:
            intermediate = malloc(sizeof(long));
            break;
        case TYPE_FLOAT:
        case TYPE_DOUBLE:
            intermediate = malloc(sizeof(double));
            break;
        default:
            return NULL;  // Unsupported type
    }

    // Copy the value to the intermediate type
    switch (from) {
        case TYPE_CHAR:
            *(long*)intermediate = *(char*)ptr;
            break;
        case TYPE_SHORT:
            *(long*)intermediate = *(short*)ptr;
            break;
        case TYPE_INT:
            *(long*)intermediate = *(int*)ptr;
            break;
        case TYPE_LONG:
            *(long*)intermediate = *(long*)ptr;
            break;
        case TYPE_UCHAR:
            *(unsigned long*)intermediate = *(unsigned char*)ptr;
            break;
        case TYPE_USHORT:
            *(unsigned long*)intermediate = *(unsigned short*)ptr;
            break;
        case TYPE_UINT:
            *(unsigned long*)intermediate = *(unsigned int*)ptr;
            break;
        case TYPE_ULONG:
            *(unsigned long*)intermediate = *(unsigned long*)ptr;
            break;
        case TYPE_POINTER:
        case TYPE_VOIDPOINTER:
        case TYPE_STRING:
            *(unsigned long*)intermediate = *(uintptr_t*)ptr;
            break;
        case TYPE_FLOAT:
            *(double*)intermediate = *(float*)ptr;
            break;
        case TYPE_DOUBLE:
            *(double*)intermediate = *(double*)ptr;
            break;
        default:
            break;
    }

    // Perform the conversion between floating-point and integer types
    switch (from) {
        case TYPE_FLOAT:
        case TYPE_DOUBLE:
            switch (to) {
                case TYPE_CHAR:
                case TYPE_SHORT:
                case TYPE_INT:
                case TYPE_LONG:
                case TYPE_POINTER:
                case TYPE_VOIDPOINTER:
                case TYPE_STRING:
                    *(long*)intermediate = (long)*(double*)intermediate;
                    break;
                case TYPE_UCHAR:
                case TYPE_USHORT:
                case TYPE_UINT:
                case TYPE_ULONG:
                    *(unsigned long*)intermediate = (unsigned long)*(double*)intermediate;
                    break;
                default:
                    break;
            }
            break;
        case TYPE_UCHAR:
        case TYPE_USHORT:
        case TYPE_UINT:
        case TYPE_ULONG:
        case TYPE_POINTER:
        case TYPE_VOIDPOINTER:
        case TYPE_STRING:
            switch (to) {
                case TYPE_FLOAT:
                case TYPE_DOUBLE:
                    *(double*)intermediate = (double)*(unsigned long*)intermediate;
                    break;
                case TYPE_CHAR:
                case TYPE_SHORT:
                case TYPE_INT:
                case TYPE_LONG:
                    *(long*)intermediate = (long)*(unsigned long*)intermediate;
                    break;
                default:
                    break;
            }
            break;
        case TYPE_CHAR:
        case TYPE_SHORT:
        case TYPE_INT:
        case TYPE_LONG:
            switch (to) {
                case TYPE_FLOAT:
                case TYPE_DOUBLE:
                    *(double*)intermediate = (double)*(long*)intermediate;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    // Allocate memory for the target type and copy the value
    switch (to) {
        case TYPE_CHAR:
            result = malloc(sizeof(char));
            *(char*)result = (char)*(long*)intermediate;
            break;
        case TYPE_SHORT:
            result = malloc(sizeof(short));
            *(short*)result = (short)*(long*)intermediate;
            break;
        case TYPE_INT:
            result = malloc(sizeof(int));
            *(int*)result = (int)*(long*)intermediate;
            break;
        case TYPE_LONG:
            result = malloc(sizeof(long));
            *(long*)result = *(long*)intermediate;
            break;
        case TYPE_UCHAR:
            result = malloc(sizeof(unsigned char));
            *(unsigned char*)result = (unsigned char)*(unsigned long*)intermediate;
            break;
        case TYPE_USHORT:
            result = malloc(sizeof(unsigned short));
            *(unsigned short*)result = (unsigned short)*(unsigned long*)intermediate;
            break;
        case TYPE_UINT:
            result = malloc(sizeof(unsigned int));
            *(unsigned int*)result = (unsigned int)*(unsigned long*)intermediate;
            break;
        case TYPE_ULONG:
            result = malloc(sizeof(unsigned long));
            *(unsigned long*)result = *(unsigned long*)intermediate;
            break;
        case TYPE_FLOAT:
            result = malloc(sizeof(float));
            *(float*)result = (float)*(double*)intermediate;
            break;
        case TYPE_DOUBLE:
            result = malloc(sizeof(double));
            *(double*)result = *(double*)intermediate;
            break;
        case TYPE_POINTER:
        case TYPE_VOIDPOINTER:
        case TYPE_STRING:
            result = malloc(sizeof(void*));
            *(uintptr_t*)result = (uintptr_t)*(unsigned long*)intermediate;
            break;
        default:
            free(intermediate);
            return NULL;  // Unsupported type
    }

    free(intermediate);
    return result;
}


void castArgValueToType(ArgInfo* destinationTypedArg, ArgInfo* sourceValueArg){
    //still need to figure out what to do about arrays and structs
    // I guess treat arrays as pointers
    // but how do we cast structs to other stuff?
    // I could imagine trying to cast a struct to a char pointer

    ArgType destinationType = destinationTypedArg->type;
    ArgType sourceType = sourceValueArg->type;

    if (sourceValueArg->pointer_depth > 0 || sourceValueArg->is_array) {
        sourceType = TYPE_POINTER;
    } else if (sourceValueArg->type == TYPE_STRUCT) { //we can't necessarily cast from a raw struct
            if (destinationTypedArg->type == TYPE_VOIDPOINTER) {
                sourceType = TYPE_POINTER;
            } else if (destinationTypedArg->is_array) {
                if (sourceValueArg->struct_info->info.arg_count > 0 && destinationTypedArg->type == sourceValueArg->struct_info->info.args[0]->type) {
                    sourceType = TYPE_POINTER;
                } else if (destinationTypedArg->type == TYPE_CHAR || destinationTypedArg->type == TYPE_UCHAR) {
                    sourceType = TYPE_POINTER;
                } else {
                    fprintf(stderr, "Error: Cannot cast a struct to an array of a different type other than char or uchar\n");
                    exit_or_restart(1);
                }
            } else {
                fprintf(stderr, "Error: Cannot cast a struct to a different type\n");
                exit_or_restart(1);
            }
    }
    if (destinationTypedArg->pointer_depth > 0 || destinationTypedArg->is_array) {
        destinationType = TYPE_POINTER;
    } else if (destinationTypedArg->type == TYPE_STRUCT) {  // we can't necessarily cast to a raw struct
            if (sourceValueArg->type == TYPE_VOIDPOINTER) {
                destinationType = TYPE_POINTER;
            } else if (sourceValueArg->is_array) {
                if (destinationTypedArg->struct_info->info.arg_count > 0 && sourceValueArg->type == destinationTypedArg->struct_info->info.args[0]->type) {
                    destinationType = TYPE_POINTER;
                } else if (sourceValueArg->type == TYPE_CHAR || sourceValueArg->type == TYPE_UCHAR) {
                    destinationType = TYPE_POINTER;
                } else {
                    fprintf(stderr, "Error: Cannot cast an array of a type other than char or uchar to a struct\n");
                    exit_or_restart(1);
                }
            } else {
                fprintf(stderr, "Error: Cannot cast a different type to a struct\n");
                exit_or_restart(1);
            }
    }
    if (sourceValueArg->type == TYPE_STRUCT) { // assuming destinationType is now TYPE_POINTER
        sourceValueArg->value->ptr_val=make_raw_value_for_struct(sourceValueArg,false);
    }
    void* convertedValue = dynamicCast(sourceValueArg->value, sourceType, destinationType);
    if (convertedValue == NULL) {
        fprintf(stderr, "Error: Failed to cast value from type %c to type %c\n", typeToChar(sourceType), typeToChar(destinationType));
        exit_or_restart(1);
    }

    if (destinationTypedArg->type == TYPE_STRUCT){ // assuming sourceType is now TYPE_POINTER
        fix_struct_pointers(destinationTypedArg, sourceValueArg->value->ptr_val); // but why aren't we using converted value at all?
    } else {
        destinationTypedArg->value = convertedValue;
    }
}

    // should we maybe descend to the lowest level of the pointer before casting so we don't share memory
    // actually probably not because casting pointer types generally does end up sharing memory

    // still, we probably haven't appropriately handled pointer depth here



// void convert_all_arrays_to_static_sized_after_function_return(FunctionCallInfo* call_info){
//     for (int i = 0; i < call_info->arg_count; i++) {
//     if (call_info->args[i]->is_array==ARRAY_SIZE_AT_ARGINFO_PTR){
//         call_info->args[i]->static_or_implied_size = get_size_for_arginfo_sized_array(&call_info->args[i]);
//         call_info->args[i]->is_array=ARRAY_STATIC_SIZE;
//     }
//     }
//     // do the same for the return value
//     if (call_info->return_var->is_array==ARRAY_SIZE_AT_ARGINFO_PTR){
//         call_info->return_var->static_or_implied_size = get_size_for_arginfo_sized_array(call_info->return_var);
//         call_info->return_var->is_array=ARRAY_STATIC_SIZE;
//     }
// // }

// void freeArgInfo(ArgInfo* arg){
//     void* temp = arg->value.ptr_val;

//     for (int i = 0; i < arg->pointer_depth; i++) {
//         void* this_level = temp;
//         temp = *(void**)temp;
//         free(this_level);
//     }

//     // The problem with freeing arrays and strings is we cannot tell if they were memalloc'ed on the heap or if they were literals in global / static, eg .data or .rodata
//     // We can choose to assume they are probably on the heap, and free them, but it will cause a segfault if they were not
//     // Or we can just decided to not free them, and let the OS clean up after the program ends

//     if (arg->is_array && (arg->type==TYPE_STRING /* || arg->type==TYPE_STRUCT */)){
//         for (int i = 0; i < arg->static_or_implied_size; i++) { // we are assuming we've already made the array be of static size
//             free(((char**)temp)[i]);
//         }
//     }

//     if (arg->is_array || arg->type==TYPE_STRING /*|| arg->type==TYPE_STRUCT*/ ){
//         free(temp);
//     }
// }

// void freeFunctionCallInfo(FunctionCallInfo* info) {
//     if (info) {
//         free(info->library_path); // Assuming this was dynamically allocated
//         free(info->function_name);
//         convert_all_arrays_to_static_sized_after_function_return(info); // get final sizes for any arrays before freeing
//         for (int i = 0; i < info->arg_count; i++) {
//             freeArgInfo(info->args[i]);
//         }
//         freeArgInfo(info->return_var);
//         free(info->args);
//         free(info);
//         }
//     }