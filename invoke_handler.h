#ifndef INVOKE_HANDLER_H
#define INVOKE_HANDLER_H

#include "types_and_utils.h" // Assuming this header defines FunctionCallInfo

// Function to invoke a dynamic function call
int invoke_dynamic_function(FunctionCallInfo* call_info, void* func);
void* make_raw_value_for_struct(ArgInfo* struct_arginfo, bool is_return);
void fix_struct_pointers(ArgInfo* struct_arg, void* raw_memory);

#endif // INVOKE_HANDLER_H
