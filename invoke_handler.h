#ifndef INVOKE_HANDLER_H
#define INVOKE_HANDLER_H

#include "types_and_utils.h" // Assuming this header defines FunctionCallInfo

// Function to invoke a dynamic function call
int invoke_dynamic_function(FunctionCallInfo* call_info, void* func);

#endif // INVOKE_HANDLER_H
