#include "return_formatter.h"
#include "types_and_utils.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "exception_handling.h"

#define ISPRINT(c) ((c) >= 32 && (c) < 127)

void print_char_with_escape(char c) {
    switch (c) {
        case '\0':
            fprintf(stdout, "\\0");
            break;
        case '\n':
            fprintf(stdout, "\\n");
            break;
        case '\r':
            fprintf(stdout, "\\r");
            break;
        case '\t':
            fprintf(stdout, "\\t");
            break;
        case '\\':
            fprintf(stdout, "\\\\");
            break;
        default:
            if (ISPRINT(c)) {
                fprintf(stdout, "%c", c);
            } else {
                fprintf(stdout, "\\x%02x", c);
            }
            break;
    }
}

void print_char_buffer(const char *buffer, size_t length) {
    for (size_t i = 0; i < length; i++) {
        unsigned char c = buffer[i];
        print_char_with_escape(c);
    }
}

void hexdump(const void *data, size_t size) {
    const unsigned char *byte = (const unsigned char *)data;
    size_t i, j;
    bool multiline = size > 16;
    if (multiline) fprintf(stdout, "(Hexvalue)\nOffset\n");

    for (i = 0; i < size; i += 16) {
        if (multiline) fprintf(stdout, "%08zx  ", i); // Offset

        // Hex bytes
        for (j = 0; j < 16; j++) {
            if (j==8) fprintf(stdout, " "); // Add space between the two halves of the hexdump
            if (i + j < size) {
                fprintf(stdout, "%02x ", byte[i + j]);
            } else {
                 if (multiline) fprintf(stdout, "   "); // Fill space if less than 16 bytes in the line
            }
        }

        if (multiline) fprintf(stdout, " ");
        if (!multiline) fprintf(stdout, "= ");

        // ASCII characters
        for (j = 0; j < 16; j++) {
            if (i + j < size) {
                fprintf(stdout, "%c", ISPRINT(byte[i + j]) ? byte[i + j] : '.');
            }
        }

        fprintf(stdout, "\n");
    }
}


    void print_arg_value(const void* value, ArgType type, size_t offset, int pointer_depth) {
    if (pointer_depth > 0) {
        value = value + offset * sizeof(void*);
        for (int j = 0; j < pointer_depth; j++) {
            value = *(void**)value;
            if (value == NULL) {
                fprintf(stdout, "(NULL pointer)");
                return;
            }
        }
        offset = 0;
    }
    //bugfix for architectures that throw a bus error when trying to read from an unaligned address
    void* alligned_copy = NULL;
    if (type!=TYPE_VOID){
    size_t size = typeToSize(type, 0);
    if (size == 0) {
        raiseException(1,  "Size of type %s is 0\n", typeToString(type));
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
            print_char_with_escape(((char*)value)[offset]);
            break;
        case TYPE_SHORT:
            fprintf(stdout, "%hd", ((short*)value)[offset]);
            break;
        case TYPE_INT:
            fprintf(stdout, "%d", ((int*)value)[offset]);
            break;
        case TYPE_LONG:
            fprintf(stdout, "%ld", ((long*)value)[offset]);
            break;
        case TYPE_UCHAR:
            fprintf(stdout, "%u",  ((unsigned char*)value)[offset]);
            break;
        case TYPE_USHORT:
            fprintf(stdout, "%hu", ((unsigned short*)value)[offset]);
            break;
        case TYPE_UINT:
            fprintf(stdout, "%u", ((unsigned int*)value)[offset]);
            break;
        case TYPE_ULONG:
            fprintf(stdout, "%lu", ((unsigned long*)value)[offset]);
            break;
        case TYPE_FLOAT:
            fprintf(stdout, "%f", ((float*)value)[offset]);
            break;
        case TYPE_DOUBLE:
            fprintf(stdout, "%lf", ((double*)value)[offset]);
            break;
        case TYPE_STRING:
            fprintf(stdout, "\"");
            print_char_buffer(((char**)value)[offset], strlen(((char**)value)[offset]));
            fprintf(stdout, "\"");
            break;
        case TYPE_VOIDPOINTER:
            // #define HEX_DIGITS (int)(2 * sizeof(void*))
            // fprintf(stdout, "0x%0*" PRIxPTR, HEX_DIGITS,(uintptr_t)((void**)value)[offset]);
            fprintf(stdout, "0x%" PRIxPTR, (uintptr_t)((void**)value)[offset]);
            break;
        case TYPE_POINTER:
            raiseException(1,  "Should not be printing pointer values directly");
        case TYPE_VOID:
            fprintf(stdout, "(void)");
            break;
        case TYPE_STRUCT:
            raiseException(1,  "Should not be printing struct values directly");
        case TYPE_BOOL:
            fprintf(stdout, "%s", ((bool*)value)[offset] ? "true" : "false");
            break;
        default:
            fprintf(stdout, "Unsupported type");
            break;
    }
    if (alligned_copy != NULL) {
        free(alligned_copy);
    }
}

    void format_and_print_arg_type(const ArgInfo* arg) {
        if (!arg->is_array) {
        fprintf(stdout, "%s", typeToString(arg->type));
        for (int i = 0; i < arg->pointer_depth; i++) {
            fprintf(stdout, "*");
        }} else { // is an array
            fprintf(stdout, "%s ", typeToString(arg->type));
            for (int i = 0; i < arg->array_value_pointer_depth; i++) {
                fprintf(stdout, "*");
            }
            if (arg->pointer_depth > 0) {
                fprintf(stdout, "(");
                for (int i = 0; i < arg->pointer_depth; i++) {
                    fprintf(stdout, "*");
                }
                fprintf(stdout, ")");
            }
            fprintf(stdout, "[%zu]", get_size_for_arginfo_sized_array(arg));
        }
    }


    //this contains an optional override for the value, which is used for printing struct fields
    void format_and_print_arg_value(const ArgInfo* arg) {  //, char* buffer, size_t buffer_size) {

        const void* value;
        value = arg->value;

        if(arg->is_outPointer && !(arg->is_array && arg->pointer_depth == 0)) {
            printf("(outpointer)");
            return;
        }

        if (arg->pointer_depth > 0 && arg->type != TYPE_STRUCT) {
            for (int j = 0; j < arg->pointer_depth; j++) {
                value = *(void**)value;
                if (arg->type!=TYPE_STRUCT && value == NULL) {
                    fprintf(stdout, "(NULL pointer)");
                    return;
                }
            }
        }
        if (arg->type==TYPE_STRUCT){
            fprintf(stdout, "{ ");
            StructInfo* struct_info = arg->struct_info;
            for (int i = 0; i < struct_info->info.arg_count; i++) {
                format_and_print_arg_type(struct_info->info.args[i]);
                fprintf(stdout, " ");
                // void* override = struct_info->value_ptrs==NULL ? NULL : struct_info->value_ptrs[i];
                format_and_print_arg_value(struct_info->info.args[i]);
                if (i < struct_info->info.arg_count - 1) {
                    fprintf(stdout, ", ");
                }
            }
            fprintf(stdout, " }");
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
                fprintf(stdout, "{ ");
                for (size_t i = 0; i < array_size; i++) {
                    print_arg_value(value, arg->type,i, arg->array_value_pointer_depth);
                    if (i < array_size - 1) {
                        fprintf(stdout, ", ");
                    }
                }
                fprintf(stdout, " }");
            }
        }
    }