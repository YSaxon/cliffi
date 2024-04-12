#ifndef MAIN_H
#define MAIN_H

void exit_or_restart(int status);
void setCodeSectionForSegfaultHandler(const char* section);
void unsetCodeSectionForSegfaultHandler();

#endif // MAIN_H