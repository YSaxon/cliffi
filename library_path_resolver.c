#include "library_path_resolver.h"
#include "exception_handling.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

#if !defined(__WIN32) && !defined(__APPLE__) && !defined(__ANDROID__)
#define use_ld_so_conf
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


#ifdef __linux__
#include <dirent.h>     // For opendir, readdir, closedir
#include <ctype.h>      // For isdigit
// Assume MAX_PATH_LENGTH is defined
// Assume file_exists(const char* path) is defined (uses stat)
// Assume str_ends_with(const char* str, const char* suffix) is defined

// Simplified version comparison: compares "X.Y.Z" strings.
// Returns >0 if v1 is newer, <0 if v2 is newer, 0 if equal or incomparable.
// A truly robust implementation (like strverscmp) is more complex.
static int compare_versions(const char* version_str1, const char* version_str2) {
    // Simple parsing for major.minor.patch
    long major1 = 0, minor1 = 0, patch1 = 0;
    long major2 = 0, minor2 = 0, patch2 = 0;

    // sscanf will stop at the first non-matching char, which is fine.
    sscanf(version_str1, "%ld.%ld.%ld", &major1, &minor1, &patch1);
    sscanf(version_str2, "%ld.%ld.%ld", &major2, &minor2, &patch2);

    if (major1 != major2) return (major1 > major2) ? 1 : -1;
    if (minor1 != minor2) return (minor1 > minor2) ? 1 : -1;
    if (patch1 != patch2) return (patch1 > patch2) ? 1 : -1;
    return 0;
}
#endif

static bool FindInPath(const char* base_path, const char* library_name_to_check, char* resolved_path) {
    // Standard direct match:
    // This is the default for non-Linux OS, or for Linux if the input isn't like "libname.so",
    // or if the Linux-specific version scan above didn't yield a result.
    snprintf(resolved_path, MAX_PATH_LENGTH, "%s/%s", base_path, library_name_to_check);
    if (file_exists(resolved_path)) {
        return true;
    }

    #if defined(__linux__)
    // On Linux, if library_name_to_check looks like a base .so name (e.g., "libfoo.so"),
    // we also try finding a versioned file (e.g., "libfoo.so.6") in the same directory,
    if (str_ends_with(library_name_to_check, ".so")) {
        DIR *dirp;
        struct dirent *entry;
        char best_match_found_path[MAX_PATH_LENGTH] = {0}; // Full path of the best match
        char current_best_version_string[MAX_PATH_LENGTH] = {0}; // "X.Y.Z" part of the best match

        size_t base_name_len = strlen(library_name_to_check);

        dirp = opendir(base_path);
        if (dirp != NULL) {
            while ((entry = readdir(dirp)) != NULL) {
                // Check if the entry starts with library_name_to_check,
                // is followed by '.', and then a digit.
                // e.g., library_name_to_check = "libc.so"
                // entry->d_name could be "libc.so.6"
                if (strncmp(entry->d_name, library_name_to_check, base_name_len) == 0 &&
                    entry->d_name[base_name_len] == '.' && // Character after base_name must be '.'
                    strlen(entry->d_name) > base_name_len + 1 && // Must have characters after the dot
                    isdigit((unsigned char)entry->d_name[base_name_len + 1])) { // First char after dot must be a digit

                    char candidate_full_path[MAX_PATH_LENGTH];
                    snprintf(candidate_full_path, MAX_PATH_LENGTH, "%s/%s", base_path, entry->d_name);

                    if (file_exists(candidate_full_path)) { // Ensure it's a valid file/symlink
                        const char* version_part = entry->d_name + base_name_len + 1; // Points to "6" in "libc.so.6"

                        if (best_match_found_path[0] == '\0' || // First match
                            compare_versions(version_part, current_best_version_string) > 0) {

                            strncpy(best_match_found_path, candidate_full_path, MAX_PATH_LENGTH -1);
                            best_match_found_path[MAX_PATH_LENGTH-1] = '\0';

                            strncpy(current_best_version_string, version_part, MAX_PATH_LENGTH -1);
                            current_best_version_string[MAX_PATH_LENGTH-1] = '\0';
                        }
                    }
                }
            }
            closedir(dirp);

            if (best_match_found_path[0] != '\0') { // If a versioned match was found by scanning
                strncpy(resolved_path, best_match_found_path, MAX_PATH_LENGTH -1);
                resolved_path[MAX_PATH_LENGTH-1] = '\0';
                return true;
            }
        }

        return false;
    }
    #endif // __linux__



    return false; // No match found
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
    const char* standard_paths[] = {"/system/lib64", "/system/lib", "/system/vendor/lib64", "/system/vendor/lib",  NULL};
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

// Function to find library in /etc/ld.so.conf file (exclude windows and mac)
#ifdef use_ld_so_conf
#include <glob.h>                   // For glob(), globfree()
#include <ctype.h>  // For isspace
#include <stddef.h> // For size_t
#define MAX_LD_SO_CONF_RECURSION 16 // Define a sensible recursion limit

/**
 * @brief Extracts the directory portion of a filepath.
 *
 * If the filepath contains '/', the function copies the part of the string
 * before the last '/' into dir_buffer.
 * - If filepath is "/foo/bar", dir_buffer becomes "/foo".
 * - If filepath is "foo/bar", dir_buffer becomes "foo".
 * - If filepath is "/foo", dir_buffer becomes "/".
 * - If filepath contains no '/', dir_buffer becomes ".".
 * - If filepath is "/", dir_buffer becomes "/".
 * The output in dir_buffer is always null-terminated, provided buffer_size > 0.
 * If buffer_size is too small to hold the result, the output will be truncated
 * and null-terminated. If buffer_size is 0, or filepath/dir_buffer is NULL,
 * the function does nothing (or ensures dir_buffer[0] is '\0' if possible).
 *
 * @param filepath The input file path.
 * @param dir_buffer The buffer to store the extracted directory path.
 * @param buffer_size The size of dir_buffer.
 */
void get_directory_part(const char* filepath, char* dir_buffer, size_t buffer_size) {
    if (!filepath || !dir_buffer || buffer_size == 0) {
        if (dir_buffer && buffer_size > 0) {
            dir_buffer[0] = '\0';
        }
        return;
    }
    // Initialize to empty string for safety, in case no conditions below are met
    // or if snprintf/strncpy fails to write anything due to buffer_size constraints.
    dir_buffer[0] = '\0';

    const char* last_slash = strrchr(filepath, '/');

    if (last_slash == NULL) { // No slash, e.g., "filename.txt"
        // Return "." for current directory
        snprintf(dir_buffer, buffer_size, "%s", ".");
    } else if (last_slash == filepath) { // Slash is the first character, e.g., "/filename.txt" or just "/"
        // Path is in root directory, or is root itself. Directory is "/"
        snprintf(dir_buffer, buffer_size, "%s", "/");
    } else { // Slash is somewhere in the middle or end
        size_t dir_len = last_slash - filepath;
        if (dir_len < buffer_size) {
            strncpy(dir_buffer, filepath, dir_len);
            dir_buffer[dir_len] = '\0'; // Null-terminate
        } else {                        // Not enough space for the full directory part, copy what fits
            strncpy(dir_buffer, filepath, buffer_size - 1);
            dir_buffer[buffer_size - 1] = '\0'; // Null-terminate
        }
    }
}

/**
 * @brief Trims leading whitespace from a string.
 *
 * Iterates from the beginning of the string, skipping whitespace characters.
 *
 * @param str The string to trim. Can be NULL.
 * @return A pointer to the first non-whitespace character in str,
 * or to the null terminator if str consists only of whitespace,
 * or NULL if str was NULL.
 */
char* trim_leading_whitespace(char* str) {
    if (!str) {
        return NULL;
    }
    while (*str && isspace((unsigned char)*str)) {
        str++;
    }
    return str;
}

/**
 * @brief Trims leading/trailing whitespace and removes comments from a string.
 *
 * This function modifies the input string 'line' in-place.
 * 1. Comments (anything from '#' to the end of the line) are removed.
 * 2. Trailing whitespace (spaces, tabs, newlines) is removed.
 * 3. Leading whitespace is skipped by advancing the returned pointer.
 *
 * @param line The string to process. Can be NULL.
 * @return A pointer to the beginning of the significant content within 'line',
 * or to the string's null terminator if the line becomes effectively empty,
 * or NULL if the input 'line' was NULL.
 * The original 'line' buffer is modified.
 */
char* trim_whitespace_and_comments(char* line) {
    if (!line) {
        return NULL;
    }

    // 1. Remove comments: find '#' and terminate string there
    char* comment_start = strchr(line, '#');
    if (comment_start != NULL) {
        *comment_start = '\0';
    }

    // 2. Trim trailing whitespace
    size_t len = strlen(line);
    while (len > 0 && isspace((unsigned char)line[len - 1])) {
        line[--len] = '\0';
    }

    // 3. Trim leading whitespace (by returning an advanced pointer)
    char* effective_line_start = line;
    while (*effective_line_start && isspace((unsigned char)*effective_line_start)) {
        effective_line_start++;
    }

    return effective_line_start;
}

static bool FindInLdSoConfFile(const char* conf_file, const char* library_name, char* resolved_path, int recursion_depth) {
    if (recursion_depth > MAX_LD_SO_CONF_RECURSION) {
        // fprintf(stderr, "Max recursion depth reached for ld.so.conf: %s\n", conf_file);
        return false;
    }

    FILE* file = fopen(conf_file, "r");
    if (!file) {
        return false;
    }

    char line_buffer[MAX_PATH_LENGTH];
    char current_conf_dir[MAX_PATH_LENGTH] = "."; // Default to current dir if conf_file is relative and has no dir part

    // Get the directory of the current configuration file to resolve relative includes
    // A robust get_directory_part is important here.
    // realpath(conf_file, NULL) could get the canonical path first, then dirname.
    // For simplicity, let's assume get_directory_part handles it:
    get_directory_part(conf_file, current_conf_dir, sizeof(current_conf_dir));

    while (fgets(line_buffer, sizeof(line_buffer), file)) {
        char* effective_line = trim_whitespace_and_comments(line_buffer); // Helper to strip comments and surrounding whitespace

        if (strlen(effective_line) == 0) { // Skip empty or comment-only lines
            continue;
        }

        // Note: ldconfig source typically tokenizes more carefully than just "include "
        // It might look for "include" as the first token, then the path.
        // For simplicity, we use strncmp.
        if (strncmp(effective_line, "include ", 8) == 0) {
            char pattern_spec[MAX_PATH_LENGTH];
            strncpy(pattern_spec, effective_line + 8, sizeof(pattern_spec) - 1);
            pattern_spec[sizeof(pattern_spec) - 1] = '\0';
            char* trimmed_pattern_spec = trim_leading_whitespace(pattern_spec);

            char full_pattern_path[MAX_PATH_LENGTH];
            if (trimmed_pattern_spec[0] == '/') { // Absolute pattern
                snprintf(full_pattern_path, sizeof(full_pattern_path), "%s", trimmed_pattern_spec);
            } else { // Relative pattern
                snprintf(full_pattern_path, sizeof(full_pattern_path), "%s/%s", current_conf_dir, trimmed_pattern_spec);
            }

            if (strpbrk(full_pattern_path, "*?[]") != NULL) { // Contains glob characters
                glob_t glob_results;
                int glob_status = glob(full_pattern_path, 0, NULL, &glob_results); // Basic flags

                if (glob_status == 0) { // Success
                    for (size_t i = 0; i < glob_results.gl_pathc; i++) {
                        if (FindInLdSoConfFile(glob_results.gl_pathv[i], library_name, resolved_path, recursion_depth + 1)) {
                            globfree(&glob_results);
                            fclose(file);
                            return true;
                        }
                    }
                } else if (glob_status == GLOB_NOMATCH) {
                    // No files matched the pattern, this is fine, continue to next line
                } else {
                    // Other glob error (GLOB_NOSPACE, GLOB_ABORTED, GLOB_ERR if flag set)
                    // fprintf(stderr, "Glob error %d for pattern: %s\n", glob_status, full_pattern_path);
                }
                globfree(&glob_results); // Crucial: free memory from glob
            } else {                     // Not a glob, just a regular include path
                if (FindInLdSoConfFile(full_pattern_path, library_name, resolved_path, recursion_depth + 1)) {
                    fclose(file);
                    return true;
                }
            }
        } else { // Not an "include" directive, treat as a path
            if (FindInPath(effective_line, library_name, resolved_path)) {
                fclose(file);
                return true;
            }
        }
    }
    fclose(file);
    return false;
}
#endif // !_WIN32

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
#ifdef use_ld_so_conf
                 || FindInLdSoConfFile("/etc/ld.so.conf", library_name, resolved_path, 0)
#endif
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
