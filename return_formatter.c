#include "return_formatter.h"
#include "types_and_utils.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    if (multiline) printf("(Hexvalue)\nOffset      ");

    for (i = 0; i < size; i += 16) {
        if (multiline) printf("%08zx  ", i); // Offset

        // Hex bytes
        for (j = 0; j < 16; j++) {
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


    void print_arg_value(const void* value, ArgType type, size_t offset) {
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
        case TYPE_POINTER:
            fprintf(stderr, "Should not be printing pointer values directly");
            exit(1);
        case TYPE_VOID:
            printf("(void)");
            break;
        default:
            printf("Unsupported type");
            break;
    }
}

    void format_and_print_arg_type(const ArgInfo* arg) {
        for (int i = 0; i < arg->pointer_depth; i++) {
            printf("*");
        }
        printf("%s", typeToString(arg->type));
        if (arg->is_array) {
            printf("[%zu]", get_size_for_arginfo_sized_array(arg));
        }
    }

    void format_and_print_arg_value(const ArgInfo* arg) {  //, char* buffer, size_t buffer_size) {
        const void* value = &(arg->value);
        if (arg->pointer_depth > 0) {
            for (int j = 0; j < arg->pointer_depth; j++) {
                value = *(void**)value;
            }
        }
        if (!arg->is_array) {
            print_arg_value(value, arg->type, 0);
            // printf("\n");
        }
        else {
            value = *(void**)value;
            size_t array_size = get_size_for_arginfo_sized_array(arg);
            if (arg->type == TYPE_CHAR){
                print_char_buffer((const char*)value, array_size);
                // print it as a string, with escape characters
            } else if (arg->type == TYPE_UCHAR) {
                hexdump(value, array_size);
            } else 
            {
                printf("{ ");
                for (size_t i = 0; i < array_size; i++) {
                    print_arg_value(value, arg->type,i);
                    if (i < array_size - 1) {
                        printf(", ");
                    }
                }
                printf(" }");
                // printf(" }\n");
            }
        }
    }