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
#endif // SHIMS_H