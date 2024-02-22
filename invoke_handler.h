#ifndef INVOKE_HANDLER_H
#define INVOKE_HANDLER_H

#include "types_and_utils.h" // Assuming this header defines FunctionCallInfo

// Function to invoke a dynamic function call
int invoke_dynamic_function(const FunctionCallInfo* call_info, ArgInfo* out_return_value);

#endif // INVOKE_HANDLER_H
