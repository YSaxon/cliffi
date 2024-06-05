#include "library_path_resolver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include "exception_handling.h"

#ifdef _WIN32
#include <windows.h>
const char* library_extension = ".dll";
const char* ENV_DELIM = ";";
#elif defined(__APPLE__)
const char* library_extension = ".dylib";
const char* ENV_DELIM = ":";
#else
const char* library_extension = ".so";
const char* ENV_DELIM = ":";
#endif

#define MAX_PATH_LENGTH 4096

// Function to check if a string ends with a specific substring
bool str_ends_with(const char* str, const char* suffix) {
    if (!str || !suffix)
        return false;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len)
        return false;
    return strncmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}

// Helper function to check if a file exists and is accessible
static bool file_exists(const char* path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

// Function to find library in a given path
static bool FindInPath(const char* base_path, const char* library_name, char* resolved_path) {
    snprintf(resolved_path, MAX_PATH_LENGTH, "%s/%s", base_path, library_name);
    return file_exists(resolved_path);
}

// Function to find library in an environment variable
static bool FindInEnvVar(const char* env_var, const char* library_name, char* resolved_path) {
    setCodeSectionForSegfaultHandler("FindInEnvVar: getenv");
    const char* env_value = getenv(env_var);
    if (!env_value) {
        return false;
    }

    setCodeSectionForSegfaultHandler("FindInEnvVar: strtok_r");
    char* search_paths = strdup(env_value);
    char* save_token;
    char* current_path = strtok_r(search_paths, ENV_DELIM, &save_token);

    while (current_path) {
        setCodeSectionForSegfaultHandler("FindInEnvVar: FindInPath");
        if (FindInPath(current_path, library_name, resolved_path)) {
            free(search_paths);
            return true;
        }
        setCodeSectionForSegfaultHandler("FindInEnvVar: strtok_r");
        current_path = strtok_r(NULL, ENV_DELIM, &save_token);
    }
    free(search_paths);
    return false;
}

// Function to find library in standard paths
static bool FindInStandardPaths(const char* library_name, char* resolved_path) {
#if defined(__ANDROID__)
    const char* standard_paths[] = {"/system/lib", "/system/lib64", "/system/vendor/lib", "/system/vendor/lib64", NULL};
#elif defined(__APPLE__)
    const char* standard_paths[] = {"/usr/lib", "/lib", "/usr/local/lib", "/opt/local/lib", "/opt/homebrew/lib", NULL};
#else
    const char* standard_paths[] = {"/usr/lib", "/lib", "/usr/local/lib", NULL};
#endif
    for (int i = 0; standard_paths[i] != NULL; i++) {
        if (FindInPath(standard_paths[i], library_name, resolved_path)) {
            return true;
        }
    }
    return false;
}

// Function to find library in /etc/ld.so.conf file
#ifndef _WIN32
static bool FindInLdSoConfFile(const char* conf_file, const char* library_name, char* resolved_path) {
    FILE* file = fopen(conf_file, "r");
    if (!file) {
        return false;
    }

    char current_directory[MAX_PATH_LENGTH];
    while (fgets(current_directory, sizeof(current_directory), file)) {
        char* end_of_line = strpbrk(current_directory, "\r\n");
        if (end_of_line) {
            *end_of_line = '\0';
        }

        if (strncmp(current_directory, "include ", 8) == 0) {
            char included_file[MAX_PATH_LENGTH];
            snprintf(included_file, sizeof(included_file), "%s", current_directory + 8);
            if (FindInLdSoConfFile(included_file, library_name, resolved_path)) {
                fclose(file);
                return true;
            }
        } else {
            if (FindInPath(current_directory, library_name, resolved_path)) {
                fclose(file);
                return true;
            }
        }
    }

    fclose(file);
    return false;
}
#endif


static bool FindSharedLibrary(const char* library_name, char* resolved_path) {

    if (library_name[0] == '/') {
        if (file_exists(library_name)) {
            strncpy(resolved_path, library_name, MAX_PATH_LENGTH);
            resolved_path[MAX_PATH_LENGTH - 1] = '\0';
            return true;
        }
        return false;
    }


        #ifdef _WIN32
    bool found = FindInEnvVar("PATH", library_name, resolved_path);
        #else
    bool found = FindInEnvVar("LD_LIBRARY_PATH", library_name, resolved_path)
                 || FindInLdSoConfFile("/etc/ld.so.conf", library_name, resolved_path)
                 || FindInStandardPaths(library_name, resolved_path);
        #endif

    return found;
}

// Function to attempt to resolve the library path
char* resolve_library_path(const char* library_name) {
    if (!library_name) {
        return NULL;
    }

    char resolved_path[MAX_PATH_LENGTH];
    if (FindSharedLibrary(library_name, resolved_path)) {
        return strdup(resolved_path);
    }

    // If we haven't found it yet, try appending the library extension and trying again
    if (!str_ends_with(library_name, library_extension)) {
        char library_name_with_extension[strlen(library_name) + strlen(library_extension) + 1];
        snprintf(library_name_with_extension, sizeof(library_name_with_extension), "%s%s", library_name, library_extension);
        if (FindSharedLibrary(library_name_with_extension, resolved_path)) {
            return strdup(resolved_path);
        }
    }

    return NULL; // Library not found
}
