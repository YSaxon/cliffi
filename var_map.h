#ifndef VAR_MAP_H
#define VAR_MAP_H

#include "types_and_utils.h"

ArgInfo* getVar(const char* name);
void setVar(const char* name, ArgInfo* value);

#endif // VAR_MAP_H