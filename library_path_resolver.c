#include "library_path_resolver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #define PATH_SEPARATOR '\\'
    #define LIB_ENV_VAR "PATH"
    const char* standard_paths[] = {"C:\\Windows\\System32", NULL};
#else
    #include <sys/stat.h>
    #include <unistd.h>
    #define PATH_SEPARATOR '/'
    #define LIB_ENV_VAR "LD_LIBRARY_PATH"
    const char* standard_paths[] = {"/usr/lib", "/lib", "/usr/local/lib", NULL};
#endif

#ifdef _WIN32
    const char* library_extension = ".dll";
#elif defined(__APPLE__)
    const char* library_extension = ".dylib";
#else
    const char* library_extension = ".so";
#endif

// Function to check if a string ends with a specific substring
int str_ends_with(const char* str, const char* suffix) {
    if (!str || !suffix)
        return 0;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len)
        return 0;
    return strncmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}

// Helper function to check if a file exists and is accessible
static int file_exists(const char* path) {
    #ifdef _WIN32
        DWORD attrib = GetFileAttributes(path);
        return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
    #else
        struct stat buffer;
        return (stat(path, &buffer) == 0);
    #endif
}

// Function to attempt to resolve the library path
char* resolve_library_path(const char* library_name) {
    if (!library_name) {
        return NULL;
    }

    // Check if the path is already absolute
    if (library_name[0] == PATH_SEPARATOR) {
        if (file_exists(library_name)) {
            return strdup(library_name);
        }
    }

    // Attempt to find the library relative to the current directory
    char* relative_path = malloc(strlen(library_name) + 3); // +3 for './', path separator, and '\0'
    snprintf(relative_path, strlen(library_name) + 3, ".%c%s", PATH_SEPARATOR, library_name);
    if (file_exists(relative_path)) {
        return relative_path;
    }
    free(relative_path);

    // Attempt to find the library in LD_LIBRARY_PATH or default paths
    const char* lib_path = getenv(LIB_ENV_VAR);
    char* search_paths = NULL;
    if (lib_path) {
        search_paths = malloc(strlen(lib_path) + 1);
        strncpy(search_paths, lib_path, strlen(lib_path) + 1);
    }
    #ifdef _WIN32
    char* path = strtok(search_paths, ";");
    #else
    char* path = strtok(search_paths, ":");
    #endif

    while (path != NULL) {
        char* full_path = malloc(strlen(path) + strlen(library_name) + 2); // +2 for path separator and '\0'
        snprintf(full_path, strlen(path) + strlen(library_name) + 2, "%s%c%s", path, PATH_SEPARATOR, library_name);
        if (file_exists(full_path)) {
            free(search_paths);
            return full_path;
        }
        free(full_path);
        #ifdef _WIN32
        path = strtok(NULL, ";");
        #else
        path = strtok(NULL, ":");
        #endif
    }
    free(search_paths);

    // If the library wasn't found in LD_LIBRARY_PATH, check standard locations
    for (int i = 0; standard_paths[i] != NULL; i++) {
        char* full_path = malloc(strlen(standard_paths[i]) + strlen(library_name) + 2); // +2 for path separator and '\0'
        snprintf(full_path, strlen(standard_paths[i]) + strlen(library_name) + 2, "%s%c%s", standard_paths[i], PATH_SEPARATOR, library_name);
        if (file_exists(full_path)) {
            return full_path;
        }
        free(full_path);
    }


    // If we haven't found it yet, try appending the platform appropriate library extension and trying again
    char* library_name_with_extension;
    if (!str_ends_with(library_name, library_extension)) {
        char library_name_with_extension[strlen(library_name) + strlen(library_extension) + 1];
        snprintf(library_name_with_extension, sizeof(library_name_with_extension), "%s%s", library_name, library_extension);
        return resolve_library_path(library_name_with_extension);
    } else {
        // Library not found
        return NULL;
    }
}