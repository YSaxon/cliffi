#ifndef RETURN_FORMATTER_H
#define RETURN_FORMATTER_H

#include "types_and_utils.h"

// void format_and_print_return_value(const ArgInfo* return_value);
void format_and_print_arg_value(const ArgInfo* arg);//, char* buffer, size_t buffer_size);
void format_and_print_arg_type(const ArgInfo* arg);
void hexdump(const void *data, size_t size);
// void print_arg_value(const void* value, ArgType type);

#endif // RETURN_FORMATTER_H