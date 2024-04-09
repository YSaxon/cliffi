
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//adapted from https://stackoverflow.com/a/69808797/10773089
char** shlex_split(const char* s, int* num_tokens) {
    char** result = NULL;
    int capacity = 0;
    int size = 0;

    char* token = NULL;
    int token_size = 0;
    int token_capacity = 0;

    char quote = '\0';
    int escape = 0;

    for (const char* p = s; *p != '\0'; p++) {
        char c = *p;

        if (escape) {
            escape = 0;
            if (quote && c != '\\' && c != quote) {
                if (token_size + 1 >= token_capacity) {
                    token_capacity = (token_capacity == 0) ? 16 : token_capacity * 2;
                    token = realloc(token, token_capacity);
                }
                token[token_size++] = '\\';
            }
            if (token_size + 1 >= token_capacity) {
                token_capacity = (token_capacity == 0) ? 16 : token_capacity * 2;
                token = realloc(token, token_capacity);
            }
            token[token_size++] = c;
        } else if (c == '\\') {
            escape = 1;
        } else if (!quote && (c == '\'' || c == '\"')) {
            quote = c;
        } else if (quote && c == quote) {
            quote = '\0';
            if (token_size == 0) {
                if (size + 1 >= capacity) {
                    capacity = (capacity == 0) ? 16 : capacity * 2;
                    result = realloc(result, capacity * sizeof(char*));
                }
                result[size++] = strdup("");
            }
        } else if (!isspace(c) || quote) {
            if (token_size + 1 >= token_capacity) {
                token_capacity = (token_capacity == 0) ? 16 : token_capacity * 2;
                token = realloc(token, token_capacity);
            }
            token[token_size++] = c;
        } else if (token_size > 0) {
            if (size + 1 >= capacity) {
                capacity = (capacity == 0) ? 16 : capacity * 2;
                result = realloc(result, capacity * sizeof(char*));
            }
            token[token_size] = '\0';
            result[size++] = token;
            token = NULL;
            token_size = 0;
            token_capacity = 0;
        }
    }

    if (token_size > 0) {
        if (size + 1 >= capacity) {
            capacity = (capacity == 0) ? 16 : capacity * 2;
            result = realloc(result, capacity * sizeof(char*));
        }
        token[token_size] = '\0';
        result[size++] = token;
    }

    if (size + 1 >= capacity) {
        capacity = (capacity == 0) ? 16 : capacity * 2;
        result = realloc(result, capacity * sizeof(char*));
    }
    result[size] = NULL;

    *num_tokens = size;
    return result;
}

int tokenize(const char* str, int* argc, char*** argv){
    *argv = shlex_split(str, argc);
    if (*argv == NULL) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return -1;
    }
    return 0;
}

