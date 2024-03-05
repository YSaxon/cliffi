#ifndef INVOKE_HANDLER_H
#define INVOKE_HANDLER_H

#include "types_and_utils.h" // Assuming this header defines FunctionCallInfo

// Function to invoke a dynamic function call
int invoke_dynamic_function(FunctionCallInfo* call_info, void* func);
ffi_type* arg_type_to_ffi_type(const ArgInfo* arg);
void* make_raw_value_for_struct(ArgInfo* struct_arginfo);//, ffi_type* struct_type);

#endif // INVOKE_HANDLER_H
