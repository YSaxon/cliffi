#include "return_formatter.h"
#include "types_and_utils.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"

void print_char_buffer(const char *buffer, size_t length) {
    for (size_t i = 0; i < length; i++) {
        unsigned char c = buffer[i];
        if (c == '\0') {
            printf("\\x00");
        } else {
            if (!isprint(c)) {
                printf("\\x%02x", c);
            } else {
                printf("%c", c);
            }
    }
    }
}

void hexdump(const void *data, size_t size) {
    const unsigned char *byte = (const unsigned char *)data;
    size_t i, j;
    bool multiline = size > 16;
    if (multiline) printf("(Hexvalue)\nOffset\n");

    for (i = 0; i < size; i += 16) {
        if (multiline) printf("%08zx  ", i); // Offset

        // Hex bytes
        for (j = 0; j < 16; j++) {
            if (j==8) printf(" "); // Add space between the two halves of the hexdump
            if (i + j < size) {
                printf("%02x ", byte[i + j]);
            } else {
                 if (multiline) printf("   "); // Fill space if less than 16 bytes in the line
            }
        }

        if (multiline) printf(" ");
        if (!multiline) printf("= ");

        // ASCII characters
        for (j = 0; j < 16; j++) {
            if (i + j < size) {
                printf("%c", isprint(byte[i + j]) ? byte[i + j] : '.');
            }
        }

        printf("\n");
    }
}


    void print_arg_value(const void* value, ArgType type, size_t offset, int pointer_depth) {
    if (pointer_depth > 0) {
        value = value + offset * sizeof(void*);
        for (int j = 0; j < pointer_depth; j++) {
            value = *(void**)value;
        }
        offset = 0;
    }
    //bugfix for architectures that throw a bus error when trying to read from an unaligned address
    void* alligned_copy = NULL;
    if (type!=TYPE_VOID){
    size_t size = typeToSize(type, 0);
    if (size == 0) {
        fprintf(stderr, "Size of type %s is 0\n", typeToString(type));
        exit_or_restart(1);
    }
    if ((size_t)value % size != 0)  {
        alligned_copy = malloc(size);
        memcpy(alligned_copy, value + offset * size, size);
        value = alligned_copy;
        offset = 0;
    }
}
    switch (type) {
        case TYPE_CHAR:
            printf("%c", ((char*)value)[offset]);
            break;
        case TYPE_SHORT:
            printf("%hd", ((short*)value)[offset]);
            break;
        case TYPE_INT:
            printf("%d", ((int*)value)[offset]);
            break;
        case TYPE_LONG:
            printf("%ld", ((long*)value)[offset]);
            break;
        case TYPE_UCHAR:
            printf("%u",  ((unsigned char*)value)[offset]);
            break;
        case TYPE_USHORT:
            printf("%hu", ((unsigned short*)value)[offset]);
            break;
        case TYPE_UINT:
            printf("%u", ((unsigned int*)value)[offset]);
            break;
        case TYPE_ULONG:
            printf("%lu", ((unsigned long*)value)[offset]);
            break;
        case TYPE_FLOAT:
            printf("%f", ((float*)value)[offset]);
            break;
        case TYPE_DOUBLE:
            printf("%lf", ((double*)value)[offset]);
            break;
        case TYPE_STRING:
            printf("\"%s\"", ((char**)value)[offset]);
            break;
        case TYPE_VOIDPOINTER:
            printf("%p", ((void**)value)[offset]);
            break;
        case TYPE_POINTER:
            fprintf(stderr, "Should not be printing pointer values directly");
            exit_or_restart(1);
        case TYPE_VOID:
            printf("(void)");
            break;
        case TYPE_STRUCT:
            fprintf(stderr, "Should not be printing struct values directly");
            exit_or_restart(1);
        default:
            printf("Unsupported type");
            break;
    }
    if (alligned_copy != NULL) {
        free(alligned_copy);
    }
}

    void format_and_print_arg_type(const ArgInfo* arg) {
        if (!arg->is_array) {
        printf("%s", typeToString(arg->type));
        for (int i = 0; i < arg->pointer_depth; i++) {
            printf("*");
        }} else { // is an array
            printf("%s ", typeToString(arg->type));
            for (int i = 0; i < arg->array_value_pointer_depth; i++) {
                printf("*");
            }
            if (arg->pointer_depth > 0) {
                printf("(");
                for (int i = 0; i < arg->pointer_depth; i++) {
                    printf("*");
                }
                printf(")");
            }
            printf("[%zu]", get_size_for_arginfo_sized_array(arg));
        }
    }


    //this contains an optional override for the value, which is used for printing struct fields
    void format_and_print_arg_value(const ArgInfo* arg) {  //, char* buffer, size_t buffer_size) {
        
        const void* value;
        value = arg->value;

        if (arg->pointer_depth > 0) {
            for (int j = 0; j < arg->pointer_depth; j++) {
                value = *(void**)value;
            }
        }
        if (arg->type==TYPE_STRUCT){
            printf("{ ");
            StructInfo* struct_info = arg->struct_info;
            for (int i = 0; i < struct_info->info.arg_count; i++) {
                format_and_print_arg_type(struct_info->info.args[i]);
                printf(" ");
                // void* override = struct_info->value_ptrs==NULL ? NULL : struct_info->value_ptrs[i];
                format_and_print_arg_value(struct_info->info.args[i]);
                if (i < struct_info->info.arg_count - 1) {
                    printf(", ");
                }
            }
            printf(" }");
        }
        else if (!arg->is_array) {
            print_arg_value(value, arg->type, 0, 0);
        } 
        else { // is an array
            value = *(void**)value; // because arrays are stored as pointers
            size_t array_size = get_size_for_arginfo_sized_array(arg);
            if(arg->array_value_pointer_depth==0){
                if (arg->type == TYPE_CHAR){
                    print_char_buffer((const char*)value, array_size);
                    // print it as a string, with escape characters
                } else if (arg->type == TYPE_UCHAR) {
                    hexdump(value, array_size);
            } else goto print_regular;
            } else print_regular: {
                printf("{ ");
                for (size_t i = 0; i < array_size; i++) {
                    print_arg_value(value, arg->type,i, arg->array_value_pointer_depth);
                    if (i < array_size - 1) {
                        printf(", ");
                    }
                }
                printf(" }");
            }
        }
    }