// library_manager.h

#ifndef LIBRARY_MANAGER_H
#define LIBRARY_MANAGER_H

void initializeLibraryManager();
void cleanupLibraryManager();
void* libManagerLoadLibrary(const char* libraryPath);
void* getLibraryHandle(const char* libraryPath);
void* getFunctionSymbol(void* libraryHandle, const char* functionName);
void closeLibrary(const char* libraryPath);
void closeAllLibraries();
void listOpenedLibraries();

#endif