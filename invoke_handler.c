#include "invoke_handler.h"
#include "return_formatter.h"
#include <stdbool.h>
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

char* ffi_status_to_string(ffi_status status) {
    switch (status) {
        case FFI_OK: return "FFI_OK";
        case FFI_BAD_TYPEDEF: return "FFI_BAD_TYPEDEF";
        case FFI_BAD_ABI: return "FFI_BAD_ABI";
        case FFI_BAD_ARGTYPE : return "FFI_BAD_ARGTYPE";
        default: return "Unknown status";
    }
}


void handle_promoting_vararg_if_necessary(ffi_type** arg_type_ptr, ArgInfo* arg, int argnum) {

    if (arg->pointer_depth > 0 || arg->is_array) return; // we don't need to promote pointers or arrays (which in functions are passed as pointers)

    size_t int_size = ffi_type_sint.size;
    ffi_type* arg_type = *arg_type_ptr;
    if (arg->type == TYPE_FLOAT) {
        arg->value.d_val = (double)arg->value.f_val;
        arg->type = TYPE_DOUBLE;
        if (arg->explicitType) {
            fprintf(stderr, "Warning: arg[%d] is a vararg so it was promoted from float to double\n", argnum);
        }
    } else if ((arg_type->type != FFI_TYPE_STRUCT
               && arg_type->type != FFI_TYPE_COMPLEX)
               && arg_type->size < int_size) {
        if (arg->type==TYPE_CHAR) {
            arg->value.i_val = (int)arg->value.c_val;
            arg->type = TYPE_INT;
        } else if (arg->type==TYPE_SHORT) {
            arg->value.i_val = (int)arg->value.s_val;
            arg->type = TYPE_INT;
        } else if (arg->type==TYPE_UCHAR) {
            arg->value.ui_val = (unsigned int)arg->value.uc_val;
            arg->type = TYPE_UINT;
        } else if (arg->type==TYPE_USHORT) {
            arg->value.ui_val = (unsigned int)arg->value.us_val;
            arg->type = TYPE_UINT;
        } else { // if its too small but not one of the above types then we don't know what to do so just fail
            fprintf(stderr, "Error: arg[%d] is a vararg but its %s type %s is of size %zu which is less than sizeof(int) which is %zu\nYou should probably %s explicit type flag", argnum, arg->explicitType ? "explicit" : "inferred", typeToString(arg->type), arg_type->size,int_size,arg->explicitType ? "correct the" : "add an");
            exit(1);
        }
        //if it was one of the above types that we COULD convert
        if (arg->explicitType) { // only bother them with a warning if they explicitly set the type
            fprintf(stderr, "Warning: arg[%d] is a vararg so it was promoted from %s to %s\n", argnum, typeToString(arg->type), typeToString(TYPE_INT));
        }
    }
    else return; // if it's not too small then we don't need to do anything

    // now we need to update the arg_type_ptr to point to the new type
    *arg_type_ptr = arg_type_to_ffi_type(arg, false);
}


// Main function to invoke a dynamic function call
int invoke_dynamic_function(FunctionCallInfo* call_info, void* func) {

    ffi_cif cif;
    ffi_type* args[call_info->info.arg_count];
    void* values[call_info->info.arg_count];
    for (int i = 0; i < call_info->info.arg_count; ++i) {
        args[i] = arg_type_to_ffi_type(&call_info->info.args[i],false);

        // if the arg is past the vararg start, we need to upgrade it if it's < sizeof(int) or a float type
        bool is_vararg = call_info->info.vararg_start!=-1 && i >= call_info->info.vararg_start;
        if (is_vararg) handle_promoting_vararg_if_necessary(&args[i], &call_info->info.args[i], i);

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
        fprintf(stderr, "ffi_prep_cif failed. Return status = %s\n", ffi_status_to_string(status));
        exit(1);
    }

    ffi_call(&cif, func, rvalue, values);

    if (call_info->info.return_var.type == TYPE_STRUCT) {
        fix_struct_pointers(&call_info->info.return_var, rvalue);
    } else if (return_type->size < ffi_type_sint.size && call_info->info.return_var.type != TYPE_VOID && call_info->info.return_var.type != TYPE_FLOAT) {
        // this is really only necessary for rare architectures, but it's a good idea to do it anyway
        if (call_info->info.return_var.type==TYPE_CHAR) {
            call_info->info.return_var.value.c_val = (char)call_info->info.return_var.value.i_val;
        } else if (call_info->info.return_var.type==TYPE_SHORT) {
            call_info->info.return_var.value.s_val = (short)call_info->info.return_var.value.i_val;
        } else if (call_info->info.return_var.type==TYPE_UCHAR) {
            call_info->info.return_var.value.uc_val = (unsigned char)call_info->info.return_var.value.ui_val;
        } else if (call_info->info.return_var.type==TYPE_USHORT) {
            call_info->info.return_var.value.us_val = (unsigned short)call_info->info.return_var.value.ui_val;
        } else {
            fprintf(stderr, "Warning: sizeof(return type) < sizeof(int), but we couldn't automatically promote it, so on some rare (BE?) architectures it may malfunction\n");
        }
    }

    return 0;
}
