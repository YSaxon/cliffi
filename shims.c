#if defined(_WIN32) || defined(_WIN64)
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    static const size_t INITIAL_SIZE = 128;
    static const size_t GROWTH_FACTOR = 2;
    if (lineptr == NULL || stream == NULL || n == NULL) {
        errno = EINVAL;
        return -1;
    }

    char *buffer = *lineptr;
    size_t size = *n;
    size_t position = 0;

    if (buffer == NULL || size == 0) {
        size = INITIAL_SIZE;
        buffer = malloc(size);
        if (buffer == NULL) {
            errno = ENOMEM;
            return -1;
        }
    }

    int c;
    while ((c = fgetc(stream)) != EOF) {
        if (position >= size - 1) {
            size_t new_size = size * GROWTH_FACTOR;
            char *new_buffer = realloc(buffer, new_size);
            if (new_buffer == NULL) {
                errno = ENOMEM;
                free(buffer);
                return -1;
            }
            buffer = new_buffer;
            size = new_size;
        }
        buffer[position++] = (char)c;
        if (c == '\n') break;
    }

    buffer[position] = '\0';
    *lineptr = buffer;
    *n = position;

    return (position == 0 && c == EOF) ? -1 : (ssize_t)position;
}

char* readline(const char* prompt) {
    printf("%s", prompt);  // Display the prompt

    char* line = NULL;
    size_t bufferSize = 0;
    ssize_t lineSize;

    lineSize = getline(&line, &bufferSize, stdin);  // Read input

    if (lineSize == -1) {
        free(line);
        return NULL;  // Error or EOF
    }

    if (lineSize > 0 && line[lineSize - 1] == '\n') {
        line[lineSize - 1] = '\0';  // Remove newline character
    }

    return line;  // Caller must free this memory
}


void using_history(void) {
    // Placeholder - no operation
}
int read_history(const char *filename) {
    // Placeholder - no operation
    return 0; // Success
}

int write_history(const char *filename) {
    // Placeholder - no operation
    return 0; // Success
}
int rl_bind_key(int key, int (*func)(int, int)) {
    // Placeholder - no operation
    return 0; // Success
}

#endif






