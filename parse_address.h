#ifndef PARSE_ADDRESS_H
#define PARSE_ADDRESS_H

ArgInfo* getPVar(void* address);
void* getAddressFromAddressStringOrNameOfCoercableVariable(const char* addressStr);

#endif // PARSE_ADDRESS_H