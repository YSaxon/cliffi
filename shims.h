#ifndef SHIMS_H
#define SHIMS_H
#if defined(_WIN32) || defined(_WIN64)
ssize_t getline(char **lineptr, size_t *n, FILE *stream);


char* readline(const char* prompt);


void using_history(void);
int read_history(const char *filename);
int write_history(const char *filename);
int rl_bind_key(int key, int (*func)(int, int));

#endif
#endif // SHIMS_H