#ifndef ARGPARSER_H
#define ARGPARSER_H

#include "types_and_utils.h" // Assume this header defines FunctionCallInfo and related types

// Parses command-line arguments into a FunctionCallInfo struct
FunctionCallInfo* parse_arguments(int argc, char* argv[]);
ArgInfo* parse_one_arg(int argc, char* argv[], int* args_used, bool is_return);

#endif // ARGPARSER_H
