#include "invoke_handler.h"
#include <string.h>

#if defined(__APPLE__)
    #include <ffi/ffi.h>
#else
    #include <ffi.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "types_and_utils.h"


// Utility function to convert ArgType to ffi_type
ffi_type* arg_type_to_ffi_type(const ArgInfo* arg) {
    if (arg->pointer_depth > 0 || arg->is_array) {
        return &ffi_type_pointer;
    }
    switch (arg->type) {
        case TYPE_CHAR: return &ffi_type_schar;
        case TYPE_SHORT: return &ffi_type_sshort;
        case TYPE_INT: return &ffi_type_sint;
        case TYPE_LONG: return &ffi_type_slong;
        case TYPE_UCHAR: return &ffi_type_uchar;
        case TYPE_USHORT: return &ffi_type_ushort;
        case TYPE_UINT: return &ffi_type_uint;
        case TYPE_ULONG: return &ffi_type_ulong;
        case TYPE_FLOAT: return &ffi_type_float;
        case TYPE_DOUBLE: return &ffi_type_double;
        case TYPE_STRING: return &ffi_type_pointer;
        case TYPE_POINTER: return &ffi_type_pointer;
        case TYPE_VOID: return &ffi_type_void;
        // Add mappings for other types
        default:
            fprintf(stderr, "Unsupported argument type.\n");
            return NULL;
    }
}

// Main function to invoke a dynamic function call
int invoke_dynamic_function(FunctionCallInfo* call_info) {
    void* lib_handle = dlopen(call_info->library_path, RTLD_LAZY);
    if (!lib_handle) {
        fprintf(stderr, "Failed to load library: %s\n", dlerror());
        return -1;
    }

    void (*func)(void);
    *(void**)(&func) = dlsym(lib_handle, call_info->function_name);
    if (!func) {
        fprintf(stderr, "Failed to find function %s in library: %s\n", call_info->function_name, dlerror());
        dlclose(lib_handle);
        return -1;
    }

    ffi_cif cif;
    ffi_type* args[call_info->arg_count];
    void* values[call_info->arg_count];
    for (int i = 0; i < call_info->arg_count; ++i) {
        args[i] = arg_type_to_ffi_type(&call_info->args[i]);
        if (!args[i]) {
            fprintf(stderr, "Failed to convert arg[%d].type = %c to ffi_type.\n", i, call_info->args[i].type);
            dlclose(lib_handle);
            return -1;
        }
        values[i] = &call_info->args[i].value;
    }

    ffi_type* return_type = arg_type_to_ffi_type(&call_info->return_var);
    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, call_info->arg_count, return_type, args) != FFI_OK) {
        fprintf(stderr, "ffi_prep_cif failed.\n");
        dlclose(lib_handle);
        return -1;
    }

    ffi_call(&cif, func, &call_info->return_var.value, values);

    if (call_info->return_var.type == TYPE_STRING && call_info->return_var.is_array == false && call_info->return_var.pointer_depth == 0) {
        call_info->return_var.value.str_val = strdup(call_info->return_var.value.str_val);
    }
    dlclose(lib_handle);
    return 0;
}
