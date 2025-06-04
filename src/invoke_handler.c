#include "invoke_handler.h"
#include "return_formatter.h"
#include <stdbool.h>
#include <string.h>

#if defined(__APPLE__)
#include <ffi/ffi.h>
#include <malloc/_malloc.h>
#else
#include <ffi.h>
#include <malloc.h>
#endif
#include "main.h"
#include "types_and_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include "exception_handling.h"


ffi_type* arg_type_to_ffi_type(const ArgInfo* arg, bool is_inside_struct); // putting this declaration here instead of header since it's only used in this file

void free_ffi_type(ffi_type* ffitype) {
    if (ffitype->type == FFI_TYPE_STRUCT) { // otherwise don't free it as it's probably an address of a static type rather than a malloc'd one
        for (int i = 0; ffitype->elements[i]; i++) {
            free_ffi_type(ffitype->elements[i]);
        }
        free(ffitype->elements);
        free(ffitype);
    }
}

ffi_type* create_raw_array_type_for_use_inside_structs(size_t n, ffi_type* array_element_type) {
    ffi_type array_type;
    ffi_type** elements;
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
    elements[n] = NULL; // NULL terminate the array

    // Initialize the array_type structure
    array_type.size = 0;      // Let libffi compute the size
    array_type.alignment = 0; // Let libffi compute the alignment
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

ffi_status get_packed_offset(const ArgInfo* struct_info, ffi_type* struct_type, size_t* offsets);

ffi_type* make_ffi_type_for_struct(const ArgInfo* arg) { // does not handle pointer_depth
    StructInfo* struct_info = arg->struct_info;
    ffi_type* struct_type = malloc(sizeof(ffi_type));
    bool ispacked = arg->struct_info->is_packed;
    // Where do I set whether it is packed or not?
    struct_type->size = 0;
    struct_type->alignment = 0;
    struct_type->type = ispacked ? FFI_TYPE_STRUCT : FFI_TYPE_STRUCT;
    struct_type->elements = calloc((struct_info->info.arg_count + 1), sizeof(ffi_type*));
    for (int i = 0; i < struct_info->info.arg_count; i++) {
        struct_type->elements[i] = arg_type_to_ffi_type(struct_info->info.args[i], true);
        if (!struct_type->elements[i]) {
            raiseException(1,  "Failed to convert struct field %d to ffi_type.\n", i);
        }
    }
    // if (!ispacked)
    ffi_status status = ffi_get_struct_offsets(FFI_DEFAULT_ABI, struct_type, NULL); // this will set size and such
    if (status != FFI_OK) {
        raiseException(1,  "Failed to get struct offsets.\n");
    }
    if (ispacked) {
        size_t offsets[struct_info->info.arg_count];
        get_packed_offset(arg, struct_type, offsets);
    }
    return struct_type;
}

size_t get_size_of_struct(const ArgInfo* arg) {
    if (arg->type != TYPE_STRUCT) {
        raiseException(1,  "get_size_of_struct called with non-struct argument.\n");
        return 0;
    }
    ffi_type* struct_type = make_ffi_type_for_struct(arg);
    size_t size_to_return = struct_type->size;
    free_ffi_type(struct_type);
    return size_to_return;
}

ffi_type* primitive_argtype_to_ffi_type(const ArgType type) {
    switch (type) {
    case TYPE_CHAR:
        return &ffi_type_schar;
    case TYPE_SHORT:
        return &ffi_type_sshort;
    case TYPE_INT:
        return &ffi_type_sint;
    case TYPE_LONG:
        return &ffi_type_slong;
    case TYPE_UCHAR:
        return &ffi_type_uchar;
    case TYPE_USHORT:
        return &ffi_type_ushort;
    case TYPE_UINT:
        return &ffi_type_uint;
    case TYPE_ULONG:
        return &ffi_type_ulong;
    case TYPE_FLOAT:
        return &ffi_type_float;
    case TYPE_DOUBLE:
        return &ffi_type_double;
    case TYPE_STRING:
        return &ffi_type_pointer;
    case TYPE_POINTER:
        return &ffi_type_pointer;
    case TYPE_VOIDPOINTER:
        return &ffi_type_pointer;
    case TYPE_VOID:
        return &ffi_type_void;
    case TYPE_BOOL:
        if (sizeof(bool) == sizeof(unsigned short)) return &ffi_type_ushort;
        else if (sizeof(bool) == sizeof(unsigned int)) return &ffi_type_uint;
        else if (sizeof(bool) == sizeof(unsigned long)) return &ffi_type_ulong;
        return &ffi_type_uchar; // this is probably right 99% of the time anyway
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
        if (inside_struct && arg->pointer_depth == 0) {
            ffi_type* element_type = arg->array_value_pointer_depth > 0 ? &ffi_type_pointer : primitive_argtype_to_ffi_type(arg->type);
            return create_raw_array_type_for_use_inside_structs(get_size_for_arginfo_sized_array(arg), element_type);
        } else {
            return &ffi_type_pointer;
        }
    } else {
        return primitive_argtype_to_ffi_type(arg->type);
    }
}

ffi_status get_packed_offset(const ArgInfo* struct_info, ffi_type* struct_type, size_t* offsets) {
    size_t offset = 0;
    for (int i = 0; struct_type->elements[i]; i++) {
        offsets[i] = offset;
        ffi_type* arg = arg_type_to_ffi_type(struct_info->struct_info->info.args[i], true);
        if (arg->size == 0) {
            size_t* inner_offsets = {0};
            if (struct_info->struct_info->info.args[i]->type == TYPE_STRUCT) {
                get_packed_offset(struct_info->struct_info->info.args[i], arg, inner_offsets);
            } else {
                ffi_get_struct_offsets(FFI_DEFAULT_ABI, arg, inner_offsets);
            }
        }
        offset += arg->size;
        free_ffi_type(arg);
    }
    struct_type->size = offset;
    struct_type->alignment = 1;
    return FFI_OK;
}

void* make_raw_value_for_struct(ArgInfo* struct_arginfo, bool is_return) { //, ffi_type* struct_type){
    ffi_type* struct_type = make_ffi_type_for_struct(struct_arginfo);
    StructInfo* struct_info = struct_arginfo->struct_info;

    size_t offsets[struct_info->info.arg_count];
    ffi_status struct_status = struct_arginfo->struct_info->is_packed ? get_packed_offset(struct_arginfo, struct_type, offsets) : ffi_get_struct_offsets(FFI_DEFAULT_ABI, struct_type, offsets);
    if (struct_status != FFI_OK) {
        raiseException(1,  "Failed to get struct offsets.\n");
    }
    void* address_to_return;
    bool outdoubleptr_dont_allocate_struct = (struct_arginfo->is_outPointer || is_return) && struct_arginfo->pointer_depth>1;

    if (outdoubleptr_dont_allocate_struct){
        printf("test allocating pointer to null\n\n");
        void* null_pointer = calloc(1,sizeof(void*));
        void** pointer_to_nullpointer = calloc(1,sizeof(void*));
        *pointer_to_nullpointer=null_pointer;
        return pointer_to_nullpointer;
    }

    else {
    void* raw_memory = calloc(1, struct_type->size);
    free_ffi_type(struct_type);
    if (!raw_memory) {
        raiseException(1,  "Failed to allocate memory for struct.\n");
    }

        for (int i = 0; i < struct_info->info.arg_count; i++) {

        if (struct_info->info.args[i]->type == TYPE_STRUCT) {
            size_t inner_size;
            if (struct_info->info.args[i]->pointer_depth == 0) {
                fprintf(stderr, "Warning, parsing a nested struct that is not a pointer type. Are you sure you meant to do this? Otherwise add a p\n");
                inner_size = get_size_of_struct(struct_info->info.args[i]);
            } else {
                inner_size = sizeof(void*);
            }

            // not necessary to set the value_ptr for a struct

            void* inner_struct_address = make_raw_value_for_struct(struct_info->info.args[i], is_return); //, struct_type->elements[i]);
            if (!is_return) memcpy(raw_memory + offsets[i], inner_struct_address, inner_size);

            // free(inner_struct_address); // this seems to make problems for embedded structs' values (probably because we've already repointed the values to this new raw_memory)

        } else if (struct_info->info.args[i]->is_array) {
            // we need to step down one layer of pointers compared to the usual handling of arrays in functions
            if (struct_info->info.args[i]->pointer_depth > 0) {
                size_t size = sizeof(void*);
                if (!is_return) memcpy(raw_memory + offsets[i], struct_info->info.args[i]->value->ptr_val, size);
                // free(struct_info->info.args[i]->value);
                struct_info->info.args[i]->value = raw_memory + offsets[i];
            } else {
                size_t size = typeToSize(struct_info->info.args[i]->type, struct_info->info.args[i]->array_value_pointer_depth) * get_size_for_arginfo_sized_array(struct_info->info.args[i]);
                if (!is_return) memcpy(raw_memory + offsets[i], struct_info->info.args[i]->value->ptr_val, size);
                // free(struct_info->info.args[i]->value);
                struct_info->info.args[i]->value = raw_memory + offsets[i];
            }

            // above are bandaid fixes for the fact that we previously decided to handle arrays as pointer types since that is how they are passed to functions as arguments

        } else {
            size_t size = typeToSize(struct_info->info.args[i]->type, struct_info->info.args[i]->pointer_depth); // passing pointer_depth to ensure we get the right size for pointers
            memcpy(raw_memory + offsets[i], struct_info->info.args[i]->value, size);
            // free(struct_info->info.args[i]->value); // we can't free this because we don't actually know if it was allocated with malloc or memcpy'd from a previous call to make_raw_value. If we want to fix this we need to add a flag to the ArgValue struct to indicate whether it was malloc'd or not
            struct_info->info.args[i]->value = raw_memory + offsets[i];
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
}

void fix_struct_pointers(ArgInfo* struct_arg, void* raw_memory) {
    StructInfo* struct_info = struct_arg->struct_info;
    size_t offsets[struct_info->info.arg_count];

    for (int i = 0; i < struct_arg->pointer_depth; i++) {
        raw_memory = *(void**)raw_memory;
        if (raw_memory == NULL) {
            struct_arg->is_outPointer = true; // if we are at a pointer depth, we should mark this as an out pointer so that we can handle it correctly later
            return; // we can't do anything with a NULL pointer, so just return
        }
    }

    ffi_type* struct_type = make_ffi_type_for_struct(struct_arg);
    ffi_status struct_status = struct_arg->struct_info->is_packed ? get_packed_offset(struct_arg, struct_type, offsets) : ffi_get_struct_offsets(FFI_DEFAULT_ABI, struct_type, offsets);
    free_ffi_type(struct_type);
    if (struct_status != FFI_OK) {
        raiseException(1,  "Failed to get struct offsets.\n");
    }

    for (int i = 0; i < struct_info->info.arg_count; i++) {
        if (struct_info->info.args[i]->type == TYPE_STRUCT) {
            fix_struct_pointers(struct_info->info.args[i], raw_memory + offsets[i]);
        } else if (!struct_info->info.args[i]->is_array) {
            struct_info->info.args[i]->value = raw_memory + offsets[i];
        } else { // is an array, so we need to copy it one level deeper since we handle arrays as pointers
            struct_info->info.args[i]->value = malloc(sizeof(*struct_info->info.args[i]->value));
            struct_info->info.args[i]->value->ptr_val = raw_memory + offsets[i];
        }
    }
}

char* ffi_status_to_string(ffi_status status) {
    switch (status) {
    case FFI_OK:
        return "FFI_OK";
    case FFI_BAD_TYPEDEF:
        return "FFI_BAD_TYPEDEF";
    case FFI_BAD_ABI:
        return "FFI_BAD_ABI";
    case FFI_BAD_ARGTYPE:
        return "FFI_BAD_ARGTYPE";
    default:
        return "Unknown status";
    }
}

void handle_promoting_vararg_if_necessary(ffi_type** arg_type_ptr, ArgInfo* arg, int argnum) {

    if (arg->pointer_depth > 0 || arg->is_array) return; // we don't need to promote pointers or arrays (which in functions are passed as pointers)

    size_t int_size = ffi_type_sint.size;
    ffi_type* arg_type = *arg_type_ptr;
    if (arg->type == TYPE_FLOAT) {
        arg->value->d_val = (double)arg->value->f_val;
        arg->type = TYPE_DOUBLE;
        if (arg->explicitType) {
            fprintf(stderr, "Warning: arg[%d] is a vararg so it was promoted from float to double\n", argnum);
        }
    } else if ((arg_type->type != FFI_TYPE_STRUCT && arg_type->type != FFI_TYPE_COMPLEX) && arg_type->size < int_size) {
        if (arg->type == TYPE_CHAR) {
            arg->value->i_val = (int)arg->value->c_val;
            arg->type = TYPE_INT;
        } else if (arg->type == TYPE_SHORT) {
            arg->value->i_val = (int)arg->value->s_val;
            arg->type = TYPE_INT;
        } else if (arg->type == TYPE_UCHAR) {
            arg->value->ui_val = (unsigned int)arg->value->uc_val;
            arg->type = TYPE_UINT;
        } else if (arg->type == TYPE_USHORT) {
            arg->value->ui_val = (unsigned int)arg->value->us_val;
            arg->type = TYPE_UINT;
        } else { // if its too small but not one of the above types then we don't know what to do so just fail
            raiseException(1,  "Error: arg[%d] is a vararg but its %s type %s is of size %zu which is less than sizeof(int) which is %zu\nYou should probably %s explicit type flag", argnum, arg->explicitType ? "explicit" : "inferred", typeToString(arg->type), arg_type->size, int_size, arg->explicitType ? "correct the" : "add an");
        }
        // if it was one of the above types that we COULD convert
        if (arg->explicitType) { // only bother them with a warning if they explicitly set the type
            fprintf(stderr, "Warning: arg[%d] is a vararg so it was promoted from %s to %s\n", argnum, typeToString(arg->type), typeToString(TYPE_INT));
        }
    } else
        return; // if it's not too small then we don't need to do anything

    // now we need to update the arg_type_ptr to point to the new type
    *arg_type_ptr = arg_type_to_ffi_type(arg, false);
}

// Main function to invoke a dynamic function call
int invoke_dynamic_function(FunctionCallInfo* call_info, void* func) {

    setCodeSectionForSegfaultHandler("invoke_dynamic_function:start");
    ffi_cif cif;
    ffi_type** args = malloc(call_info->info.arg_count * sizeof(ffi_type*));
    void** values = malloc(call_info->info.arg_count * sizeof(void*));
    if (args == NULL || values == NULL) {
        if (args != NULL) free(args);
        if (values != NULL) free(values);
        raiseException(1,  "Memory allocation failed in invoke_dynamic_function.\n");
        return -1;
    }
    for (int i = 0; i < call_info->info.arg_count; ++i) {
        args[i] = arg_type_to_ffi_type(call_info->info.args[i], false);

        // if the arg is past the vararg start, we need to upgrade it if it's < sizeof(int) or a float type
        bool is_vararg = call_info->info.vararg_start != -1 && i >= call_info->info.vararg_start;
        if (is_vararg) handle_promoting_vararg_if_necessary(&args[i], call_info->info.args[i], i);

        if (!args[i]) {
            if (args != NULL) free(args);
            if (values != NULL) free(values);
            raiseException(1,  "Failed to convert arg[%d].type = %c to ffi_type.\n", i, call_info->info.args[i]->type);
            return -1;
        }
        if (call_info->info.args[i]->type != TYPE_STRUCT
            // || call_info->info.args[i]->is_outPointer && call_info->info.args[i]->pointer_depth>1
        ) { //|| call_info->info.args[i]->pointer_depth == 0) {
            values[i] = call_info->info.args[i]->value;
        } else {
            values[i] = make_raw_value_for_struct(call_info->info.args[i], call_info->info.args[i]->is_outPointer);
        }
    }

    ffi_type* return_type = arg_type_to_ffi_type(call_info->info.return_var, false);

    void* rvalue;
    if (call_info->info.return_var->type == TYPE_STRUCT) {
        free(call_info->info.return_var->value);
        rvalue = call_info->info.return_var->value = make_raw_value_for_struct(call_info->info.return_var, true); // this also handles pointer_depth
    } else {
        rvalue = call_info->info.return_var->value;
    }

    setCodeSectionForSegfaultHandler("invoke_dynamic_function:ffi_prep");

    ffi_status status;
    if (call_info->info.vararg_start != -1) {
        fprintf(stderr, "vararg start: %d\n", call_info->info.vararg_start);
        status = ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, call_info->info.vararg_start, call_info->info.arg_count, return_type, args);
    } else {
        status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, call_info->info.arg_count, return_type, args);
    }

    if (status != FFI_OK) {
        if (args != NULL) free(args);
        if (values != NULL) free(values);
        raiseException(1,  "ffi_prep_cif failed. Return status = %s\n", ffi_status_to_string(status));
        return -1;
    }

    // in x64, the values array gets messed up, so we need to copy the values to a new array so we can fix the struct pointers after the call
    #ifdef __x86_64__
    void** values_copy = malloc(call_info->info.arg_count * sizeof(void*));
    for (int i = 0; i < call_info->info.arg_count; ++i) {
        values_copy[i] = values[i];
    }
    #endif


    setCodeSectionForSegfaultHandler("invoke_dynamic_function:ffi_call");

    ffi_call(&cif, func, rvalue, values);

    setCodeSectionForSegfaultHandler("invoke_dynamic_function:after ffi_call");

    free_ffi_type(return_type);
    for (int i = 0; i < call_info->info.arg_count; ++i) {
        free_ffi_type(args[i]);
    }

    #ifdef __x86_64__
    free(values);
    values = values_copy;
    #endif

    for (int i = 0; i < call_info->info.arg_count; ++i) {
        if (call_info->info.args[i]->type == TYPE_STRUCT) {
            fix_struct_pointers(call_info->info.args[i], values[i]);
        }
    }
    if (call_info->info.return_var->type == TYPE_STRUCT) {
        fix_struct_pointers(call_info->info.return_var, rvalue);
    }
#if defined(__s390x__)
    else if (return_type->size < ffi_type_slong.size) {
        if (call_info->info.return_var->type == TYPE_CHAR) {
            call_info->info.return_var->value->c_val = (char)call_info->info.return_var->value->l_val;
        } else if (call_info->info.return_var->type == TYPE_SHORT) {
            call_info->info.return_var->value->s_val = (short)call_info->info.return_var->value->l_val;
        } else if (call_info->info.return_var->type == TYPE_UCHAR) {
            call_info->info.return_var->value->uc_val = (unsigned char)call_info->info.return_var->value->ul_val;
        } else if (call_info->info.return_var->type == TYPE_USHORT) {
            call_info->info.return_var->value->us_val = (unsigned short)call_info->info.return_var->value->ul_val;
        } else if (call_info->info.return_var->type == TYPE_INT) {
            call_info->info.return_var->value->i_val = (int)call_info->info.return_var->value->l_val;
        } else if (call_info->info.return_var->type == TYPE_UINT) {
            call_info->info.return_var->value->ui_val = (unsigned int)call_info->info.return_var->value->ul_val;
        } else if (call_info->info.return_var->type == TYPE_BOOL) {
            call_info->info.return_var->value->b_val = (bool)call_info->info.return_var->value->l_val;
        }
    }
#elif defined(__mips__)
    else if (return_type->size < ffi_type_sint.size && call_info->info.return_var->type != TYPE_VOID && call_info->info.return_var->type != TYPE_FLOAT) {
        if (call_info->info.return_var->type == TYPE_CHAR) {
            call_info->info.return_var->value->c_val = (char)call_info->info.return_var->value->i_val;
        } else if (call_info->info.return_var->type == TYPE_SHORT) {
            call_info->info.return_var->value->s_val = (short)call_info->info.return_var->value->i_val;
        } else if (call_info->info.return_var->type == TYPE_UCHAR) {
            call_info->info.return_var->value->uc_val = (unsigned char)call_info->info.return_var->value->ui_val;
        } else if (call_info->info.return_var->type == TYPE_USHORT) {
            call_info->info.return_var->value->us_val = (unsigned short)call_info->info.return_var->value->ui_val;
        } else if (call_info->info.return_var->type == TYPE_BOOL) {
            call_info->info.return_var->value->b_val = (bool)call_info->info.return_var->value->ui_val;
        }
    }
#endif
    if (args != NULL) free(args);
    if (values != NULL) free(values);
    unsetCodeSectionForSegfaultHandler();

    return 0;
}
