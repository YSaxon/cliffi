// library_manager.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "library_manager.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

typedef struct {
    char* libraryPath;
    void* handle;
} LibraryEntry;

typedef struct {
    LibraryEntry* entries;
    size_t count;
} LibraryMap;

LibraryMap libraryMap;

void initializeLibraryManager() {
    libraryMap.entries = NULL;
    libraryMap.count = 0;
}

void cleanupLibraryManager() {
    for (size_t i = 0; i < libraryMap.count; i++) {
        closeLibrary(libraryMap.entries[i].libraryPath);
        free(libraryMap.entries[i].libraryPath);
    }
    free(libraryMap.entries);
}

void* libManagerLoadLibrary(const char* libraryPath) {
    void* handle = NULL;

#ifdef _WIN32
    handle = LoadLibrary(libraryPath);
#else
    handle = dlopen(libraryPath, RTLD_LAZY);
#endif

    if (handle != NULL) {
        LibraryEntry* entry = NULL;
        for (size_t i = 0; i < libraryMap.count; i++) {
            if (strcmp(libraryMap.entries[i].libraryPath, libraryPath) == 0) {
                entry = &libraryMap.entries[i];
                break;
            }
        }

        if (entry == NULL) {
            libraryMap.entries = realloc(libraryMap.entries, (libraryMap.count + 1) * sizeof(LibraryEntry));
            entry = &libraryMap.entries[libraryMap.count];
            entry->libraryPath = strdup(libraryPath);
            libraryMap.count++;
        }

        entry->handle = handle;
    }

    return handle;
}

// void* getLibraryHandle(const char* libraryPath) {
//     for (size_t i = 0; i < libraryMap.count; i++) {
//         if (strcmp(libraryMap.entries[i].libraryPath, libraryPath) == 0) {
//             return libraryMap.entries[i].handle;
//         }
//     }
//     return NULL;
// }

void* getFunctionSymbol(void* libraryHandle, const char* functionName) {
    void* symbol = NULL;

#ifdef _WIN32
    symbol = GetProcAddress((HMODULE)libraryHandle, functionName);
#else
    symbol = dlsym(libraryHandle, functionName);
#endif

    return symbol;
}

void closeLibrary(const char* libraryPath) {
    for (size_t i = 0; i < libraryMap.count; i++) {
        if (strcmp(libraryMap.entries[i].libraryPath, libraryPath) == 0) {
            void* handle = libraryMap.entries[i].handle;
            if (handle != NULL) {
#ifdef _WIN32
                FreeLibrary((HMODULE)handle);
#else
                dlclose(handle);
#endif
                libraryMap.entries[i].handle = NULL;
            }
            break;
        }
    }
}

void closeAllLibraries() {
    for (size_t i = 0; i < libraryMap.count; i++) {
        void* handle = libraryMap.entries[i].handle;
        if (handle != NULL) {
#ifdef _WIN32
            FreeLibrary((HMODULE)handle);
#else
            dlclose(handle);
#endif
            libraryMap.entries[i].handle = NULL;
        }
    }
}

void listOpenedLibraries() {
    printf("Opened libraries:\n");
    for (size_t i = 0; i < libraryMap.count; i++) {
        if (libraryMap.entries[i].handle != NULL) {
            printf("- %s\n", libraryMap.entries[i].libraryPath);
        }
    }
}