#ifndef MAIN_H
#define MAIN_H

void raiseException(int status, char* formatstr, ...);
void setCodeSectionForSegfaultHandler(const char* section);
void unsetCodeSectionForSegfaultHandler();

#endif // MAIN_H