#include "exception_handling.h"
#include "types_and_utils.h"
#include "var_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parse_address.h"
#include "shims.h"

ArgInfo* getPVar(void* address) {
    ArgInfo* var = (ArgInfo*)calloc(1, sizeof(ArgInfo));
    var->type = TYPE_VOIDPOINTER;
    var->explicitType = true;
    var->value = malloc(sizeof(*var->value));
    var->value->ptr_val = address;
    return var;
}

void storeOffsetForLibLoadedAtAddress(void* libhandle, void* address) {
    char* varname;
    asprintf(&varname, "liboffset_%p", libhandle);
    setVar(varname, getPVar(address));
}

void* getAddressFromStoredOffsetRelativeToLibLoadedAtAddress(void* libhandle, const char* addressStr) {
    char* varname;
    asprintf(&varname, "liboffset_%p", libhandle);
    ArgInfo* offset = getVar(varname);
    if (offset == NULL) {
        raiseException(1,  "Error: No offset stored for library loaded at address %p\n. Try again after running calculate_offset.", libhandle);
        return NULL;
    }

    //Should we use a TRY block here to reformulate the error message?
    void* address = getAddressFromAddressStringOrNameOfCoercableVariable(addressStr);
    if (address == NULL) {
        return NULL;
    }

    return (void*)((uintptr_t)offset->value->ptr_val + (uintptr_t)address);

}

void* tryGetAddressFromAddressStringOrNameOfCoercableVariable(const char* addressStr) {
    void* address = NULL;

    if (addressStr == NULL || strlen(addressStr) == 0) {
        return NULL;
    }

    if (addressStr[0] == '-') {
        return NULL;
    }

    addressStr = strdup(addressStr); // copy the string so we can modify it

    // check for a + or * operand
    char* operand_str = strchr(addressStr, '+');
    if (operand_str == NULL) {
        operand_str = strchr(addressStr, '*'); // delayed inner recursive method gets calculated first
    }
    if (operand_str != NULL) {
        char operand = *operand_str;
        *operand_str = '\0';
        operand_str++;
        void* operand_1 = tryGetAddressFromAddressStringOrNameOfCoercableVariable(addressStr);
        void* operand_2 = tryGetAddressFromAddressStringOrNameOfCoercableVariable(operand_str);
        // fprintf(stderr,"Address calculation: %p %c %p\n", operand_1, operand, operand_2);
        if (operand_1 == NULL || operand_2 == NULL) {
            return NULL;
        }
        if (operand == '*') {
            return (void*)((uintptr_t)operand_1 * (uintptr_t)operand_2);
        } else if (operand == '+') {
            return (void*)((uintptr_t)operand_1 + (uintptr_t)operand_2);
        } else {
            raiseException(1,  "Error: Should be impossible to reach this code, %c", operand);
        }
    }

    if (isHexFormat(addressStr) || isAllDigits(addressStr)) {
        if (sizeof(void*) <= sizeof(long)) {
            address = (void*)(uintptr_t)strtoul(addressStr, NULL, 0);
        } else {
            address = (void*)(uintptr_t)strtoull(addressStr, NULL, 0);
        }
    } else {
        // if it's not a number, it must be a variable name
        ArgInfo* var = getVar(addressStr);
        if (var == NULL) {
            return NULL;
        } else {
            switch (var->type) {
            case TYPE_INT:
                address = (void*)(uintptr_t) * (int*)dereferencePointerLevels(var->value, var->pointer_depth);
                break;
            case TYPE_LONG:
                address = (void*)(uintptr_t) * (long*)dereferencePointerLevels(var->value, var->pointer_depth);
                break;
            case TYPE_UINT:
                address = (void*)(uintptr_t) * (unsigned int*)dereferencePointerLevels(var->value, var->pointer_depth);
                break;
            case TYPE_ULONG:
                address = (void*)(uintptr_t) * (unsigned long*)dereferencePointerLevels(var->value, var->pointer_depth);
                break;
            case TYPE_VOIDPOINTER:
                address = dereferencePointerLevels(var->value->ptr_val, var->pointer_depth);
                break;
            default:
                if (var->is_array) {
                    address = dereferencePointerLevels(var->value->ptr_val, var->pointer_depth);
                    break;
                } else {
                    return NULL;
                }
            }
        }
        if (var->pointer_depth > 0) {
            fprintf(stderr, "Warning: %s is specified with 'p' indirection. We are dereferencing it %d level(s) and using %p as the address\n", addressStr, var->pointer_depth, address);
        }
    }
    free((void*)addressStr);
    return address;
}

void* getAddressFromAddressStringOrNameOfCoercableVariable(const char* addressStr) {
    void* address = NULL;

    if (addressStr == NULL || strlen(addressStr) == 0) {
        raiseException(1,  "Memory address cannot be empty.\n");
    }

    if (addressStr[0] == '-') {
        raiseException(1,  "Memory address cannot be negative and variables can't start with a dash.\n");
    }

    addressStr = strdup(addressStr); // copy the string so we can modify it

    // check for a + or * operand
    char* operand_str = strchr(addressStr, '+');
    if (operand_str == NULL) {
        operand_str = strchr(addressStr, '*'); // delayed inner recursive method gets calculated first
    }
    if (operand_str != NULL) {
        char operand = *operand_str;
        *operand_str = '\0';
        operand_str++;
        void* operand_1 = getAddressFromAddressStringOrNameOfCoercableVariable(addressStr);
        void* operand_2 = getAddressFromAddressStringOrNameOfCoercableVariable(operand_str);
        // fprintf(stderr,"Address calculation: %p %c %p\n", operand_1, operand, operand_2);
        if (operand_1 == NULL || operand_2 == NULL) {
            raiseException(1,  "Error: Invalid addition, one or more strings was not a valid address.\n");
        }
        if (operand == '*') {
            return (void*)((uintptr_t)operand_1 * (uintptr_t)operand_2);
        } else if (operand == '+') {
            return (void*)((uintptr_t)operand_1 + (uintptr_t)operand_2);
        } else {
            raiseException(1,  "Error: Should be impossible to reach this code, %c", operand);
        }
    }

    if (isHexFormat(addressStr) || isAllDigits(addressStr)) {
        if (sizeof(void*) <= sizeof(long)) {
            address = (void*)(uintptr_t)strtoul(addressStr, NULL, 0);
        } else {
            address = (void*)(uintptr_t)strtoull(addressStr, NULL, 0);
        }
    } else {
        // if it's not a number, it must be a variable name
        ArgInfo* var = getVar(addressStr);
        if (var == NULL) {
            raiseException(1,  "%s is neither a valid pointer address nor an existing variable.\n", addressStr);
            return NULL;
        } else {
            switch (var->type) {
            case TYPE_INT:
                address = (void*)(uintptr_t) * (int*)dereferencePointerLevels(var->value, var->pointer_depth);
                break;
            case TYPE_LONG:
                address = (void*)(uintptr_t) * (long*)dereferencePointerLevels(var->value, var->pointer_depth);
                break;
            case TYPE_UINT:
                address = (void*)(uintptr_t) * (unsigned int*)dereferencePointerLevels(var->value, var->pointer_depth);
                break;
            case TYPE_ULONG:
                address = (void*)(uintptr_t) * (unsigned long*)dereferencePointerLevels(var->value, var->pointer_depth);
                break;
            case TYPE_VOIDPOINTER:
                address = dereferencePointerLevels(var->value->ptr_val, var->pointer_depth);
                break;
            default:
                if (var->is_array) {
                    address = dereferencePointerLevels(var->value->ptr_val, var->pointer_depth);
                    break;
                } else {
                    raiseException(1,  "Error: %s is not a (void*) type, (nor any other type that could be coerced to a pointer).\n", addressStr);
                    return NULL;
                }
            }
        }
        if (var->pointer_depth > 0) {
            fprintf(stderr, "Warning: %s is specified with 'p' indirection. We are dereferencing it %d level(s) and using %p as the address\n", addressStr, var->pointer_depth, address);
        }
    }
    free((void*)addressStr);
    return address;
}