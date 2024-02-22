#ifndef LIBRARY_PATH_RESOLVER_H
#define LIBRARY_PATH_RESOLVER_H

// Function to resolve the library path. Returns dynamically allocated string that must be freed by the caller.
char* resolve_library_path(const char* library_name);

#endif // LIBRARY_PATH_RESOLVER_H
