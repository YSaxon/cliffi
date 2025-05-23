#ifndef SHIMS_H
#define SHIMS_H
#include <stdarg.h>
#if defined(_WIN32) || defined(_WIN64)


#include <stddef.h>
#include <stdio.h>

ssize_t getline(char **lineptr, size_t *n, FILE *stream);


char* readline(const char* prompt);


void using_history(void);
int read_history(const char *filename);
int write_history(const char *filename);
int rl_bind_key(int key, int func);

typedef struct command {
    char* line;
    int line_length;
} HIST_ENTRY;

#define history_length 0
#define rl_complete 0

HIST_ENTRY* history_get(int index);
int add_history(const char *line);

#endif

// Check if we're on a POSIX system that might support vasprintf directly
#if defined(__unix__) || defined(__unix) || defined(unix)
#include <unistd.h>  // Include for POSIX Operating System API
#include <features.h> // Typically for Linux, to include GNU extensions
#endif
// Feature test macro to see if vasprintf is available
#if (defined(__APPLE__) && defined(__MACH__)) || (defined(_GNU_SOURCE) && defined(HAVE_VASPRINTF)) || defined(__ANDROID__)
#define HAVE_VASPRINTF
#else
int vasprintf(char **strp, const char *fmt, va_list ap);
int asprintf(char **strp, const char *fmt, ...);
#endif


#endif // SHIMS_H

