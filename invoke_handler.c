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
#include "types_and_utils.h"

ffi_type* arg_type_to_ffi_type(const ArgInfo* arg, bool is_inside_struct); // putting this declaration here instead of header since it's only used in this file

ffi_type* create_raw_array_type_for_use_inside_structs(size_t n, ffi_type *array_element_type) {
    ffi_type array_type;
    ffi_type **elements;
    size_t i;

    // Allocate memory for the elements array with an extra slot for the NULL terminator
    elements = malloc((n + 1) * sizeof(ffi_type*));
    if (elements == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    // Populate the elements array with the type of each element
    for (i = 0; i < n; ++i) {
        elements[i] = array_element_type;
    }
    elements[n] = NULL;  // NULL terminate the array

    // Initialize the array_type structure
    array_type.size = 0;  // Let libffi compute the size
    array_type.alignment = 0;  // Let libffi compute the alignment
    array_type.type = FFI_TYPE_STRUCT;
    array_type.elements = elements;

    // Dynamically allocate a ffi_type to hold the array_type and return it
    ffi_type* type_ptr = malloc(sizeof(ffi_type));
    if (type_ptr == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        free(elements);
        return NULL;
    }
    *type_ptr = array_type;

    return type_ptr;
}

ffi_type* make_ffi_type_for_struct(const ArgInfo* arg){ // does not handle pointer_depth
    StructInfo* struct_info = arg->struct_info;
    ffi_type* struct_type = malloc(sizeof(ffi_type));
    //Where do I set whether it is packed or not?
    struct_type->size = 0;
    struct_type->alignment = 0;
    struct_type->type = FFI_TYPE_STRUCT;
    struct_type->elements = calloc((struct_info->info.arg_count + 1),sizeof(ffi_type*));
    for (int i = 0; i < struct_info->info.arg_count; i++) {
        struct_type->elements[i] = arg_type_to_ffi_type(&struct_info->info.args[i], true);
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


ffi_type* primitive_argtype_to_ffi_type(const ArgType type) {
    switch (type) {
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

// Utility function to convert ArgType to ffi_type
ffi_type* arg_type_to_ffi_type(const ArgInfo* arg, bool inside_struct) {

    if (arg->pointer_depth > 0) {
        return &ffi_type_pointer;
    } else if (arg->type == TYPE_STRUCT) {
        return make_ffi_type_for_struct(arg);
    } else if (arg->is_array) {
        if (inside_struct && arg->pointer_depth==0) return create_raw_array_type_for_use_inside_structs(get_size_for_arginfo_sized_array(arg), primitive_argtype_to_ffi_type(arg->type));
        else return &ffi_type_pointer;
    } else {
        return primitive_argtype_to_ffi_type(arg->type);
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

    struct_info->value_ptrs = calloc(struct_info->info.arg_count, sizeof(void*));
    if (!struct_info->value_ptrs) {
        fprintf(stderr, "Failed to allocate memory for struct value pointers.\n");
        exit(1);
    }

    for (int i = 0; i < struct_info->info.arg_count; i++) {

       if (struct_info->info.args[i].type == TYPE_STRUCT) {
            size_t inner_size;
            if (struct_info->info.args[i].pointer_depth == 0) {
                fprintf(stderr, "Warning, parsing a nested struct that is not a pointer type. Are you sure you meant to do this? Otherwise add a p\n");
                ffi_type* inner_struct_type = make_ffi_type_for_struct(&struct_info->info.args[i]);
                inner_size = inner_struct_type->size;
            } else {
                inner_size = sizeof(void*);
            }

            // not necessary to set the value_ptr for a struct

            void* inner_struct_address = make_raw_value_for_struct(&struct_info->info.args[i]);//, struct_type->elements[i]);
            memcpy(raw_memory+offsets[i], inner_struct_address, inner_size);

            free(inner_struct_address);


        } else if (struct_info->info.args[i].is_array) {
            // we need to step down one layer of pointers compared to the usual handling of arrays in functions
            if (struct_info->info.args[i].pointer_depth > 0) {
                fprintf(stderr, "Warning, parsing array pointers within structs is not fully tested, attempting to parse the struct pointer as one shallower pointer depth than usual\n");
                size_t size = sizeof(void*);
                memcpy(raw_memory+offsets[i], struct_info->info.args[i].value.ptr_val, size);
                struct_info->value_ptrs[i] = raw_memory+offsets[i];
            }
            else {
                fprintf(stderr, "Warning, parsing raw arrays within structs is not fully tested\n");
                size_t size = typeToSize(struct_info->info.args[i].type) * get_size_for_arginfo_sized_array(&struct_info->info.args[i]);
                memcpy(raw_memory+offsets[i], struct_info->info.args[i].value.ptr_val, size);
                struct_info->value_ptrs[i] = raw_memory+offsets[i];
            }
                
            // above are bandaid fixes for the fact that we previously decided to handle arrays as pointer types since that is how they are passed to functions as arguments

        } else copy_primitive: {
            size_t size = typeToSize(struct_info->info.args[i].type);
            memcpy(raw_memory+offsets[i], &struct_info->info.args[i].value, size);
            struct_info->value_ptrs[i] = raw_memory+offsets[i];
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


void fix_struct_pointers(ArgInfo* struct_arg, void* raw_memory) {
    StructInfo* struct_info = struct_arg->struct_info;
    size_t offsets[struct_info->info.arg_count];

    for (int i = 0; i < struct_arg->pointer_depth; i++) {
        raw_memory = *(void**)raw_memory;
    }

    ffi_type* struct_type = make_ffi_type_for_struct(struct_arg);
    ffi_status struct_status = ffi_get_struct_offsets(FFI_DEFAULT_ABI, struct_type, offsets);
    if (struct_status != FFI_OK) {
        fprintf(stderr, "Failed to get struct offsets.\n");
        exit(1);
    }

    for (int i = 0; i < struct_info->info.arg_count; i++) {
        if (struct_info->info.args[i].type == TYPE_STRUCT) {
            fix_struct_pointers(&struct_info->info.args[i], raw_memory+offsets[i]);
        } else {
            struct_info->value_ptrs[i] = raw_memory+offsets[i];
        }
    }
}


// Main function to invoke a dynamic function call
int invoke_dynamic_function(FunctionCallInfo* call_info, void* func) {

    ffi_cif cif;
    ffi_type* args[call_info->info.arg_count];
    void* values[call_info->info.arg_count];
    for (int i = 0; i < call_info->info.arg_count; ++i) {
        args[i] = arg_type_to_ffi_type(&call_info->info.args[i],false);
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

    ffi_type* return_type = arg_type_to_ffi_type(&call_info->info.return_var, false);

    void* rvalue;
    if (call_info->info.return_var.type == TYPE_STRUCT) {
        rvalue = make_raw_value_for_struct(&call_info->info.return_var); // this also handles pointer_depth
    } else {
        rvalue = &call_info->info.return_var.value;
    }


    ffi_status status;
    if (call_info->info.vararg_start!=-1) {
        fprintf(stderr,"vararg start: %d\n", call_info->info.vararg_start);
        status = ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, call_info->info.vararg_start, call_info->info.arg_count, return_type, args);
    } else {
        status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, call_info->info.arg_count, return_type, args);
    }

    if (status != FFI_OK) {
        fprintf(stderr, "ffi_prep_cif failed.\n");
        return -1;
    }

    ffi_call(&cif, func, rvalue, values);

    if (call_info->info.return_var.type == TYPE_STRUCT) {
        fix_struct_pointers(&call_info->info.return_var, rvalue);
    }

    return 0;
}
