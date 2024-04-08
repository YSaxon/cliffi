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


void* loadLibraryDirectly(const char* libraryPath) {
    #ifdef _WIN32
    void* handle = LoadLibrary(libraryPath);
    if (!handle) {
            fprintf(stderr, "Failed to load library: %lu\n", GetLastError());
            return NULL;
    }
#else
    void* handle = dlopen(libraryPath, RTLD_LAZY);
    if (!handle) {
            fprintf(stderr, "Failed to load library: %s\n", dlerror());
            return NULL;
    }
#endif
return handle;
}

LibraryEntry* getLibraryEntry(const char* libraryPath) {
    for (size_t i = 0; i < libraryMap.count; i++) {
        if (strcmp(libraryMap.entries[i].libraryPath, libraryPath) == 0) {
            return &libraryMap.entries[i];
        }
    }
    return NULL;
}

void setLibraryEntry(const char* libraryPath, void* handle) {
    LibraryEntry* entry = getLibraryEntry(libraryPath);
    if (entry == NULL) {
        libraryMap.entries = realloc(libraryMap.entries, (libraryMap.count + 1) * sizeof(LibraryEntry));
        entry = &libraryMap.entries[libraryMap.count];
        entry->libraryPath = strdup(libraryPath);
        libraryMap.count++;
    }
    entry->handle = handle;
}

void* getOrLoadLibrary(const char* libraryPath) {

    LibraryEntry* entry = getLibraryEntry(libraryPath);
    if (entry != NULL) {
        return entry->handle;
    }

    void* handle = loadLibraryDirectly(libraryPath);
    if (handle != NULL) {
        setLibraryEntry(libraryPath, handle);
    }

    return handle;
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