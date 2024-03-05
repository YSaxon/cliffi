#include "invoke_handler.h"
#include "return_formatter.h"
#include <string.h>

#if defined(__APPLE__)
    #include <malloc/_malloc.h>
    #include <ffi/ffi.h>
#else
    #include <malloc.h>
    #include <ffi.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "types_and_utils.h"

ffi_type* arg_type_to_ffi_type(const ArgInfo* arg); // putting this declaration here instead of header since it's only used in this file

ffi_type* make_ffi_type_for_struct(const ArgInfo* arg){
    StructInfo* struct_info = arg->struct_info;
    ffi_type* struct_type = malloc(sizeof(ffi_type));
    //Where do I set whether it is packed or not?
    struct_type->size = 0;
    struct_type->alignment = 0;
    struct_type->type = FFI_TYPE_STRUCT;
    struct_type->elements = calloc((struct_info->info.arg_count + 1),sizeof(ffi_type*));
    for (int i = 0; i < struct_info->info.arg_count; i++) {
        struct_type->elements[i] = arg_type_to_ffi_type(&struct_info->info.args[i]);
        if (!struct_type->elements[i]) {
            fprintf(stderr, "Failed to convert struct field %d to ffi_type.\n", i);
            exit(1);
        }
    }
    ffi_status status = ffi_get_struct_offsets(FFI_DEFAULT_ABI, struct_type, NULL); // this will set size and such
    if (status != FFI_OK) {
        fprintf(stderr, "Failed to get struct offsets.\n");
        exit(1);
    }
    return struct_type;
}

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
        case TYPE_STRUCT: return make_ffi_type_for_struct(arg);
        // Add mappings for other types
        default:
            fprintf(stderr, "Unsupported argument type.\n");
            return NULL;
    }
}
void* make_raw_value_for_struct(ArgInfo* struct_arginfo){//, ffi_type* struct_type){
    ffi_type* struct_type = make_ffi_type_for_struct(struct_arginfo);
    StructInfo* struct_info = struct_arginfo->struct_info;
    
    size_t offsets[struct_info->info.arg_count];
    ffi_status struct_status = ffi_get_struct_offsets(FFI_DEFAULT_ABI, struct_type, offsets);
    if (struct_status != FFI_OK) {
        fprintf(stderr, "Failed to get struct offsets.\n");
        exit(1);
    }

    void* raw_memory = calloc(1,struct_type->size);
    if (!raw_memory) {
        fprintf(stderr, "Failed to allocate memory for struct.\n");
        exit(1);
    }
    for (int i = 0; i < struct_info->info.arg_count; i++) {
        if (struct_info->info.args[i].type != TYPE_STRUCT) {
            size_t size = typeToSize(struct_info->info.args[i].type);
            memcpy(raw_memory+offsets[i], &struct_info->info.args[i].value, size);
        } else { // is TYPE_STRUCT
            size_t inner_size;
            if (struct_info->info.args[i].pointer_depth == 0) {
                fprintf(stderr, "Warning, parsing a nested struct that is not a pointer type. Are you sure you meant to do this? Otherwise add a p\n");
                ffi_type* inner_struct_type = make_ffi_type_for_struct(&struct_info->info.args[i]);
                inner_size = inner_struct_type->size;
            } else {
                inner_size = sizeof(void*);
            }



            void* inner_struct_address = make_raw_value_for_struct(&struct_info->info.args[i]);//, struct_type->elements[i]);
            memcpy(raw_memory+offsets[i], inner_struct_address, inner_size);

            free(inner_struct_address);


        }
    }


    // now recurse through the pointer_depth to set the pointers
    void* address_to_return = raw_memory;
    for (int i = 0; i < struct_arginfo->pointer_depth; i++) {
        void* temp = malloc(sizeof(void*)); // meaning size of a pointer
        memcpy(temp, &address_to_return, sizeof(void*));
        address_to_return = temp;
    }

    return address_to_return; // if no pointers this is a pointer to the actual bytes
}

// Main function to invoke a dynamic function call
int invoke_dynamic_function(FunctionCallInfo* call_info, void* func) {
    if (!func) {
        fprintf(stderr, "Failed to find function %s in library: %s\n", call_info->function_name, dlerror());
        return -1;
    }

    ffi_cif cif;
    ffi_type* args[call_info->info.arg_count];
    void* values[call_info->info.arg_count];
    for (int i = 0; i < call_info->info.arg_count; ++i) {
        args[i] = arg_type_to_ffi_type(&call_info->info.args[i]);
        if (!args[i]) {
            fprintf(stderr, "Failed to convert arg[%d].type = %c to ffi_type.\n", i, call_info->info.args[i].type);
            return -1;
        }
        if (call_info->info.args[i].type != TYPE_STRUCT){//|| call_info->info.args[i].pointer_depth == 0) {
            values[i] = &call_info->info.args[i].value;
        } else {
            values[i] = make_raw_value_for_struct(&call_info->info.args[i]);
        }
    }

    ffi_type* return_type = arg_type_to_ffi_type(&call_info->info.return_var);
    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, call_info->info.arg_count, return_type, args) != FFI_OK) {
        fprintf(stderr, "ffi_prep_cif failed.\n");
        return -1;
    }

    ffi_call(&cif, func, &call_info->info.return_var.value, values);

    return 0;
}
