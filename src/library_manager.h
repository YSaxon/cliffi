// library_manager.h

#ifndef LIBRARY_MANAGER_H
#define LIBRARY_MANAGER_H

void cleanupLibraryManager();
void* getOrLoadLibrary(const char* libraryPath);
void closeLibrary(const char* libraryPath);
void closeAllLibraries();
void listOpenedLibraries();

#endif