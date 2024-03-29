#include "library_path_resolver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

    // Check if the path is already absolute or relative
    if (library_name[0] == '/' || library_name[0] == '.' || strstr(library_name, "./") == library_name || strstr(library_name, "../") == library_name) {
        // Inside resolve_library_path, after checking if the path exists
        if (file_exists(library_name)) {
            return strdup(library_name);
        }
    }

    // Attempt to find the library in LD_LIBRARY_PATH or default paths
    const char* ld_lib_path = getenv("LD_LIBRARY_PATH");
    char* search_paths = ld_lib_path ? strdup(ld_lib_path) : NULL;
    char* path = strtok(search_paths, ":");

    while (path != NULL) {
        char* full_path = malloc(strlen(path) + strlen(library_name) + 2); // +2 for '/' and '\0'
        sprintf(full_path, "%s/%s", path, library_name);
        if (file_exists(full_path)) {
            free(search_paths);
            return full_path; // Caller is responsible for freeing this memory
        }
        free(full_path);
        path = strtok(NULL, ":");
    }
    free(search_paths);

    // If the library wasn't found in LD_LIBRARY_PATH, check standard locations
    const char* standard_paths[] = {"/usr/lib", "/lib", NULL};
    for (int i = 0; standard_paths[i] != NULL; i++) {
        char* full_path = malloc(strlen(standard_paths[i]) + strlen(library_name) + 2); // +2 for '/' and '\0'
        sprintf(full_path, "%s/%s", standard_paths[i], library_name);
        if (file_exists(full_path)) {
            return full_path; // Caller is responsible for freeing this memory
        }
        free(full_path);
    }

    // Library not found
    return NULL;
}
