#ifndef SYSTEM_CONSTANTS_H
#define SYSTEM_CONSTANTS_H

#ifdef _WIN32
#define DEFAULT_LIBRARY_EXTENSION ".dll"
#define DEFAULT_LIBC_NAME "ucrtbase"
#define ENV_DELIM ";"
#elif defined(__APPLE__)
#define DEFAULT_LIBRARY_EXTENSION ".dylib"
#define DEFAULT_LIBC_NAME "libSystem"
#define ENV_DELIM ":"
#else
#define DEFAULT_LIBRARY_EXTENSION ".so"
#define DEFAULT_LIBC_NAME "libc"
#define ENV_DELIM ":"
#endif

#endif // SYSTEM_CONSTANTS_H 