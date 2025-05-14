#ifndef PARSE_ADDRESS_H
#define PARSE_ADDRESS_H

#include "types_and_utils.h"

ArgInfo* getPVar(void* address);
void* getAddressFromAddressStringOrNameOfCoercableVariable(const char* addressStr);
void* tryGetAddressFromAddressStringOrNameOfCoercableVariable(const char* addressStr);
void* getAddressFromStoredOffsetRelativeToLibLoadedAtAddress(void* libhandle, const char* addressStr);
void storeOffsetForLibLoadedAtAddress(void* libhandle, void* address);

#endif // PARSE_ADDRESS_H