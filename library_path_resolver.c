#include "library_path_resolver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
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
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

// Function to attempt to resolve the library path
char* resolve_library_path(const char* library_name) {
    if (!library_name) {
        return NULL;
    }

    // Check if the path is already absolute
    if (library_name[0] == '/') {
        if (file_exists(library_name)) {
            return strdup(library_name);
        }
    }

    // Attempt to find the library relative to the current directory
    char* relative_path = malloc(strlen(library_name) + 3); // +3 for './', '/', and '\0'
    sprintf(relative_path, "./%s", library_name);
    if (file_exists(relative_path)) {
        return relative_path;
    }
    free(relative_path);

    // Attempt to find the library in LD_LIBRARY_PATH or default paths
    const char* ld_lib_path = getenv("LD_LIBRARY_PATH");
    char* search_paths = ld_lib_path ? strdup(ld_lib_path) : NULL;
    char* path = strtok(search_paths, ":");

    while (path != NULL) {
        char* full_path = malloc(strlen(path) + strlen(library_name) + 2); // +2 for '/' and '\0'
        sprintf(full_path, "%s/%s", path, library_name);
        if (file_exists(full_path)) {
            free(search_paths);
            return full_path;
        }
        free(full_path);
        path = strtok(NULL, ":");
    }
    free(search_paths);

    // If the library wasn't found in LD_LIBRARY_PATH, check standard locations
    const char* standard_paths[] = {"/usr/lib", "/lib", "/usr/local/lib", 
    #ifdef __ANDROID__
        "/system/lib", "/system/lib64", "/system/vendor/lib", "/system/vendor/lib64",
    #endif
    NULL};
    for (int i = 0; standard_paths[i] != NULL; i++) {
        char* full_path = malloc(strlen(standard_paths[i]) + strlen(library_name) + 2); // +2 for '/' and '\0'
        sprintf(full_path, "%s/%s", standard_paths[i], library_name);
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