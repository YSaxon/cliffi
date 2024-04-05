// library_manager.h

#ifndef LIBRARY_MANAGER_H
#define LIBRARY_MANAGER_H

void initializeLibraryManager();
void cleanupLibraryManager();
void* getOrLoadLibrary(const char* libraryPath);
void* loadLibraryDirectly(const char* libraryPath);
void closeLibrary(const char* libraryPath);
void closeAllLibraries();
void listOpenedLibraries();

#endif