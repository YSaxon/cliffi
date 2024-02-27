#include "return_formatter.h"
#include "types_and_utils.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void format_and_print_return_value(const ArgInfo* return_value){
    printf("Function returned: ");
    format_and_print_arg_value(return_value);
}


    void format_and_print_arg_value(const ArgInfo* arg) {  //, char* buffer, size_t buffer_size) {
        const void* value = &(arg->value);
        // char* formatted_value = buffer;
        if (arg->pointer_depth > 0) {
            printf("Pointer Depth: %d, ", arg->pointer_depth);
            for (int j = 0; j < arg->pointer_depth; j++) {
                value = *(void**)value;
            }
        }
        if (arg->is_array) {
            value = *(void**)value;
            printf("Array Size: %zu, ", arg->array_size);
            char* format_specifier = typeToFormatSpecifier(arg->type);
            int format_specifier_len = strlen(format_specifier);

            // size_t format_string_len = 7 + // for the "{}" and the null terminator
            //                            (format_specifier_len * arg->array_size) + // for the format specifiers
            //                            (2 * (arg->array_size - 1)); // for the commas
            // char format_string[format_string_len];
            // format_string[0] = '\0';
            // strcat(format_string, "{ ");
            // for (size_t i = 0; i < arg->array_size; i++) {
            //     strcat(format_string, format_specifier);
            //     if (i < arg->array_size - 1) {
            //         strcat(format_string, ", ");
            //     }
            // }
            // strcat(format_string, " }");
            
            printf("{ ");
            for (size_t i = 0; i < arg->array_size; i++) {
                void* array_element = (void*)value + (i * typeToSize(arg->type));
                print_arg_value(array_element, arg->type);
                if (i < arg->array_size - 1) {
                    printf(", ");
                }
            }
            printf(" }\n");
        } else {
            print_arg_value(value, arg->type);
            printf("\n");
        }
        }



    void print_arg_value(const void* value, ArgType type) {
    switch (type) {
        case TYPE_CHAR:
            printf("%c", *(char*)value);
            break;
        case TYPE_SHORT:
            printf("%hd", *(short*)value);
            break;
        case TYPE_INT:
            printf("%d", *(int*)value);
            break;
        case TYPE_LONG:
            printf("%ld", *(long*)value);
            break;
        case TYPE_UCHAR:
            printf("%u", (unsigned)*(unsigned char*)value);
            break;
        case TYPE_USHORT:
            printf("%hu", *(unsigned short*)value);
            break;
        case TYPE_UINT:
            printf("%u", *(unsigned int*)value);
            break;
        case TYPE_ULONG:
            printf("%lu", *(unsigned long*)value);
            break;
        case TYPE_FLOAT:
            printf("%f", *(float*)value);
            break;
        case TYPE_DOUBLE:
            printf("%lf", *(double*)value);
            break;
        case TYPE_STRING:
            printf("\"%s\"", *(char**)value);
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

// char* argvalue_to_string()