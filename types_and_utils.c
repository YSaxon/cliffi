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

ArgType infer_arg_type_single(const char* argval){
    if (!argval || strlen(argval) == 0) return TYPE_UNKNOWN;
    if (argval[0] == '"' || argval[0] == '\'') return TYPE_STRING;
    if (isFloatingPoint(argval)){
            if (endsWith(argval, 'D') || endsWith(argval, 'd')) return TYPE_DOUBLE;
            if (endsWith(argval, 'F') || endsWith(argval, 'f')) return TYPE_FLOAT;
         return TYPE_FLOAT;
         }
    if (isAllDigits(argval)) {
            if (endsWith(argval, 'L') || endsWith(argval, 'l')) return TYPE_LONG;
            if (endsWith(argval, 'U') || endsWith(argval, 'u')) return TYPE_UINT;
            if (endsWith(argval, 'I') || endsWith(argval, 'i')) return TYPE_INT;
        return TYPE_INT;
        }
    if (isHexFormat(argval)) {
        size_t length = strlen(argval);
        if (length <= 4) return TYPE_UCHAR;
        if (length <= 6) return TYPE_USHORT;
        if (length <= 10) return TYPE_UINT;
        return TYPE_ULONG;
    }

    // Add more rules as needed
    return TYPE_STRING; // Default fallback
}

void infer_arg_type_from_value(ArgInfo* arg, const char* argval) {
    arg->explicitType = false;
    arg->pointer_depth = 0;
    
    // infer array type by presence of commas
        // check that for every substring, infer_arg_type returns the same? or just use the first type?
    if (strchr(argval, ',') != NULL) {
        char* rest = strdup(argval);
        char* token = strtok_r(rest, ",", &rest);
        ArgType first_type = infer_arg_type_single(token);
        while (token != NULL) {
            ArgType next_type = infer_arg_type_single(token);
            if (next_type != first_type) {
                fprintf(stderr, "Warning: In argstr %s, multiple types found in comma delimitted list. We'll use the first type %c\n", argval, typeToChar(first_type));
                break;
            }
            token = strtok_r(rest, ",", &rest);
        }
        arg->type=first_type;
        arg->is_array = ARRAY_STATIC_SIZE_UNSET; // we'll set the size later anyway, just as if it were specified as an array, but with size unspecified
    } else {
        arg->type = infer_arg_type_single(argval);
        arg->is_array = NOT_ARRAY;
    }

   

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

typedef struct {
                size_t temp_array_size;
                void* array;
                } ArgnumSizedButAlsoImplicitlySizedArray;

void handle_array_arginfo_conversion(ArgInfo* arg, const char* argStr){


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



    if ((strcmp(argStr, "0") == 0 || strcmp(argStr, "NULL") == 0 || strcmp(argStr, "null") == 0)){
        if (arg->is_array==ARRAY_STATIC_SIZE) {
            arg->value.ptr_val = calloc(arg->array_size.static_size, typeToSize(arg->type));
            return;
        } else if (arg->is_array==ARRAY_SIZE_AT_ARGNUM) {
            // fprintf(stderr, "Warning: We have not yet implemented initializing null arrays of size pointed to by another argument, so this will be a null pointer for now\n");
            // we've now implemented this in second_pass_arginfo_ptr_sized_null_array_initialization, so we can just return
            arg->value.ptr_val = NULL;
            return;
        } else {
            fprintf(stderr, "Error: Argstr %s is interpreted as NULL. We cannot initialize a null array with no sizing info. If you WANT a null pointer you should use the pointer flag instead\n", argStr);
            exit(1);
        }
    }



    // three different cases for array argStr
    // 1. single value that is a number, which is the count of the array <-- removed on account of being duplicative with the unspace syntax we use for return values
    // 2. single value that is a hex string, which is the raw values for the array
    // 3. comma delimitted list of values for the array

    // Step 1: Split string by commas and count substrings
    int count = 1;
    size_t array_size_implicit;
    void* array_values;
    size_t size_of_type = typeToSize(arg->type);
    

    for (const char* p = argStr; *p; p++) {
        if (*p == ',') count++;
    }


    if (count == 1){
        if (isHexFormat(argStr))
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
            array_size_implicit = hexstring_bytes / size_of_type;
            array_values = hex_string_to_pointer(argStr);
        }
        else
        {
            fprintf(stderr, "Warning: In argstr %s, no valid hex value found, no static or dynamic size found, and no commas found to delimit values. We'll attempt to parse it as a single element array, but that's probably not what you intended.\n", argStr);
            goto parseascommadelimited; 
        }
    } else parseascommadelimited: { // if we didn't already parse it as a hex string, then we'll parse it as a comma delimitted list of values
        array_size_implicit = count;
        array_values = calloc(count, size_of_type);
        char* rest = strdup(argStr);
        for (int i = 0; i < count; i++) {
            char* token = strtok_r(rest, ",", &rest);
            if (token == NULL) {
                fprintf(stderr, "Error: Failed to tokenize array string %s, probably an off by one error\n", argStr);
                exit(1);
            }
            void* convertedValue = convert_to_type(arg->type, token);
            memcpy(array_values + (i * size_of_type), convertedValue, size_of_type);
            free(convertedValue);
        }
    }
    if (arg->is_array==ARRAY_STATIC_SIZE_UNSET){
        arg->is_array = ARRAY_STATIC_SIZE;
        arg->array_size.static_size = array_size_implicit;
        arg->value.ptr_val = array_values;
    } else if (arg->is_array==ARRAY_STATIC_SIZE){
        size_t explicit_size = arg->array_size.static_size;
        size_t implicit_size = array_size_implicit;
        if (explicit_size < implicit_size){
            fprintf(stderr, "Warning: Array was specified to have size %zu, but the value implies a size of %zu. Setting array size to the explicit size (values may be truncated)\n", explicit_size, implicit_size);
            arg->value.ptr_val = array_values;
            arg->array_size.static_size = explicit_size;
        } else if (explicit_size > implicit_size){
            fprintf(stderr, "Warning: Array was specified to have size %zu, but the value implies a size of %zu. Filling the rest of the array with 0s\n", explicit_size, implicit_size);
            arg->value.ptr_val = calloc(explicit_size, size_of_type);
            memcpy(arg->value.ptr_val, array_values, implicit_size * size_of_type);
            free(array_values);
        } else {
            arg->value.ptr_val = array_values;
            arg->array_size.static_size = explicit_size;
        }
    } else if (arg->is_array==ARRAY_SIZE_AT_ARGNUM){
        // fprintf(stderr, "Warning: we are initializing an argnum size_t sized array with the implicit size %zu implied by the value it is being initialized with. On the second pass of the parser, we will reallocate the array, but this may lead to truncation or \n");
        ArgnumSizedButAlsoImplicitlySizedArray *temp_array = malloc(sizeof(ArgnumSizedButAlsoImplicitlySizedArray));
        temp_array->temp_array_size = array_size_implicit;
        temp_array->array = array_values;
        arg->value.ptr_val = temp_array;
        // arg->array_size.static_size = array_size_implicit; <-- DONT DO THAT, IT WILL OVERWRITE THE ARGNUM!
        // alternatively we could add a field to the arginfo to store the implicit size
    }
    else {
        fprintf(stderr, "Error: Unsupported array size mode %d\n", arg->is_array);
        exit(1);
    }
}


void second_pass_arginfo_ptr_sized_null_array_initialization_inner(ArgInfo* arg){
        // first we have to traverse the pointer_depths to get to the actual array
        void* value = arg->value.ptr_val;
        void* parent = &arg->value;
        for (int j = 0; j < arg->pointer_depth; j++) {
            parent = value;
            value = *(void**)value;
        }
        if (value==NULL){
            size_t size = get_size_for_arginfo_sized_array(arg);
            * (void**) parent = calloc(size, typeToSize(arg->type));
        } else {
            // fprintf(stderr, "Warning: Array was specified to have its size be at arginfo ptr, but the array was already initialized with a non-null value. We will not reallocate the array, but this may lead to unexpected behavior\n");
            // realloc the array to the correct size?
            // but no way to be sure that the array will end with zeros then
            // size_t size = get_size_for_arginfo_sized_array(arg);
            // * (void**) parent = realloc(value, size);

            ArgnumSizedButAlsoImplicitlySizedArray temp_array = *(ArgnumSizedButAlsoImplicitlySizedArray*) value;

            size_t implicit_size = temp_array.temp_array_size;
            size_t explicit_size = get_size_for_arginfo_sized_array(arg);


            if (explicit_size < implicit_size){
                fprintf(stderr, "Warning: Array was specified by its size_t arg to have size %zu, but the value implies a size of %zu. Setting array size to the explicit size (values may be truncated)\n", explicit_size, implicit_size);
                * (void**) parent = temp_array.array;
            } else if (explicit_size > implicit_size){
                fprintf(stderr, "Warning: Array was specified by its size_t arg to have size %zu, but the value implies a size of %zu. Filling the rest of the array with 0s\n", explicit_size, implicit_size);
                * (void**) parent = calloc(explicit_size, typeToSize(arg->type));
                memcpy(* (void**) parent, temp_array.array, implicit_size * typeToSize(arg->type));
                free(temp_array.array);
            } else {
                * (void**) parent = temp_array.array;
            }
            free(value);
        }
}

void second_pass_arginfo_ptr_sized_null_array_initialization(FunctionCallInfo* info){
    for (int i = 0; i < info->arg_count; i++) {
        if (info->args[i].is_array==ARRAY_SIZE_AT_ARGINFO_PTR){
        second_pass_arginfo_ptr_sized_null_array_initialization_inner(&info->args[i]);
        }
    }
    if (info->return_var.is_array==ARRAY_SIZE_AT_ARGINFO_PTR){
        second_pass_arginfo_ptr_sized_null_array_initialization_inner(&info->return_var);
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

void convert_argnum_sized_array_to_arginfo_ptr(ArgInfo* arg,FunctionCallInfo* functionInfo){
    if (arg->is_array==ARRAY_SIZE_AT_ARGNUM){
        if (arg->array_size.argnum_of_size_t_to_be_replaced > functionInfo->arg_count){
            fprintf(stderr, "Error: array was specified to have its size_t be at argnum %d, but there are only %d args\n", arg->array_size.argnum_of_size_t_to_be_replaced, functionInfo->arg_count);
            exit(1);
        }
        else if (arg->array_size.argnum_of_size_t_to_be_replaced < 0){
            fprintf(stderr, "Error: array was specified to have its size_t be at argnum %d, but argnums must be positive\n", arg->array_size.argnum_of_size_t_to_be_replaced);
            exit(1);
        } else {
            arg->is_array = ARRAY_SIZE_AT_ARGINFO_PTR;
            if (arg->array_size.argnum_of_size_t_to_be_replaced==0) arg->array_size.arginfo_of_size_t = (ArgInfo*)&functionInfo->return_var; // 0 is the return value
            else arg->array_size.arginfo_of_size_t= &functionInfo->args[arg->array_size.argnum_of_size_t_to_be_replaced-1]; // -1 because the argnums are 1 indexed
        }
    }
}

void convert_all_arrays_to_arginfo_ptr_sized_after_parsing(FunctionCallInfo* functionInfo){
    for (int i = 0; i < functionInfo->arg_count; i++) {
        if (functionInfo->args[i].is_array==ARRAY_SIZE_AT_ARGNUM){
            convert_argnum_sized_array_to_arginfo_ptr(&functionInfo->args[i], functionInfo);
        }
    }
    if (functionInfo->return_var.is_array==ARRAY_SIZE_AT_ARGNUM){
        convert_argnum_sized_array_to_arginfo_ptr(&functionInfo->return_var, functionInfo);
    }
}

size_t get_size_for_arginfo_sized_array(const ArgInfo* arg){
    switch(arg->is_array){
        case ARRAY_SIZE_AT_ARGINFO_PTR:
            switch (arg->array_size.arginfo_of_size_t->type) {
                case TYPE_SHORT:
                    return (size_t)arg->array_size.arginfo_of_size_t->value.s_val;
                case TYPE_INT:
                    return (size_t)arg->array_size.arginfo_of_size_t->value.i_val;
                case TYPE_LONG:
                    return (size_t)arg->array_size.arginfo_of_size_t->value.l_val;
                case TYPE_UCHAR:
                    return (size_t)arg->array_size.arginfo_of_size_t->value.uc_val;
                case TYPE_USHORT:
                    return (size_t)arg->array_size.arginfo_of_size_t->value.us_val;
                case TYPE_UINT:
                    return (size_t)arg->array_size.arginfo_of_size_t->value.ui_val;
                case TYPE_ULONG:
                    return (size_t)arg->array_size.arginfo_of_size_t->value.ul_val;
                default:
                    fprintf(stderr, "Error: array was specified to have its size_t be another argument, but the arg at that position is not a numeric type\n");
                    exit(1);
        }
        case ARRAY_STATIC_SIZE:
            // fprintf(stderr,"Warning: getSizeForSizeTArray was called on an array with static size\n");
            return arg->array_size.static_size;
        case ARRAY_STATIC_SIZE_UNSET:
            fprintf(stderr,"Error: getSizeForSizeTArray was called on an array on static_unset mode\n");
            exit(1);
        case ARRAY_SIZE_AT_ARGNUM:
            fprintf(stderr,"Error: getSizeForSizeTArray was called on an arginfo with is_array ARRAY_SIZE_AT_ARGNUM. Call convert_all_arrays_to_arginfo_ptr_sized_after_parsing() first.\n");
            exit(1);
        case NOT_ARRAY:
            fprintf(stderr,"Error: getSizeForSizeTArray was called on an arginfo with is_array NOT_ARRAY\n");
            exit(1);
        default:
            fprintf(stderr, "Error: getSizeForSizeTArray was called on an arginfo with unsupported is_array mode %d\n", arg->is_array);
            exit(1);
    }
}

void convert_all_arrays_to_static_sized_after_function_return(FunctionCallInfo* call_info){
    // First, for simplicity, lets convert any size_t referenced arrays to their actual size
    for (int i = 0; i < call_info->arg_count; i++) { // get final sizes for any arrays before freeing, although TODO we should just make them static on function return for simplicity
    if (call_info->args[i].is_array==ARRAY_SIZE_AT_ARGINFO_PTR){
        call_info->args[i].array_size.static_size = get_size_for_arginfo_sized_array(&call_info->args[i]);
        call_info->args[i].is_array=ARRAY_STATIC_SIZE;
    }
    }
    // do the same for the return value
    if (call_info->return_var.is_array==ARRAY_SIZE_AT_ARGINFO_PTR){
        call_info->return_var.array_size.static_size = get_size_for_arginfo_sized_array(&call_info->return_var);
        call_info->return_var.is_array=ARRAY_STATIC_SIZE;
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
    void* temp = arg->value.ptr_val; 

    for (int i = 0; i < arg->pointer_depth; i++) {
        void* this_level = temp;
        temp = *(void**)temp;
        free(this_level);
    }

    if (arg->is_array && (arg->type==TYPE_STRING /* || arg->type==TYPE_STRUCT */)){
        for (int i = 0; i < arg->array_size.static_size; i++) { // we are assuming we've already made the array be of static size
            free(((char**)temp)[i]);
        }
    }

    if (arg->is_array || arg->type==TYPE_STRING /*|| arg->type==TYPE_STRUCT*/ ){
        free(temp);
    }

}

void freeFunctionCallInfo(FunctionCallInfo* info) {
    if (info) {
        free(info->library_path); // Assuming this was dynamically allocated
        free(info->function_name);
        for (int i = 0; i < info->arg_count; i++) {
            freeArgInfo(&info->args[i]);
        }
        freeArgInfo(&info->return_var);
        free(info->args);
        free(info);
        }
    }