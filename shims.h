#ifndef SHIMS_H
#define SHIMS_H
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

//shim vasprintf
// #if (!defined(_GNU_SOURCE))
#undef vasprintf
#define vasprintf(p, f, a) \
size_t size = vsprintf(NULL, f, a); \
*p = malloc(size); \
if (*p) { \
    vsprintf(*p, f, a); \
    size = strlen(*p); \
} else { \
    size = 0; \
} \
size

#undef asprintf
#define asprintf(p, f, ...) \
size_t size = sprintf(NULL, f, __VA_ARGS__); \
*p = malloc(size); \
if (*p) { \
    sprintf(*p, f, __VA_ARGS__); \
    size = strlen(*p); \
} else { \
    size = 0; \
} \
size
// #endif


#endif // SHIMS_H

