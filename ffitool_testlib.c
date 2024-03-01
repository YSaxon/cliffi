#include <malloc/_malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple function that adds two integers
int add(int a, int b) {
    return a + b;
}

// Function that concatenates two strings
const char* concat(const char* a, const char* b) {
    int total_length = strlen(a) + strlen(b);
    char* result = malloc(total_length + 1);
    result[0] = '\0'; // Initialize the string
    strcpy(result, a);
    strcat(result, b);
    return result;
}

// Function that multiplies two floating-point numbers
double multiply(double a, double b) {
    return a * b;
}

// A function that takes a pointer to an integer, increments it, and returns the result
int increment_at_pointer(int* a) {
    if (a) {
        (*a)++;
        return *a;
    }
    return 0;
}

int* get_array_of_int(int a, size_t size) {
    int* arr = calloc(size, sizeof(int));
    for (size_t i = 0; i < size; i++) {
        arr[i] = a;
    }
    return arr;
}

// A function that returns a static string
char* get_message() {
    return "Hello, ffitool!"; // this maybe shows us we need to handle string literals in data?
}

// Struct for demonstration
typedef struct {
    int x;
    double y;
} Point;

// Function that modifies a struct
void modify_point(Point* p, int x, double y) {
    if (p) {
        p->x = x;
        p->y = y;
    }
}

//Function that takes an array of integers and returns the sum
int sum_array(int* arr, int size) {
    int sum = 0;
    for (int i = 0; i < size; i++) {
        sum += arr[i];
    }
    return sum;
}

// Function that demonstrates deeper pointer depths
int increment_at_pointer_pointer(int** pp) {
    if(pp && *pp) {
        (**pp)++;
        return **pp;
    }
    return -1;
}

// Function returning a dynamically allocated array of doubles
double* get_array_of_doubles(size_t size) {
    double* arr = (double*) malloc(size * sizeof(double));
    for(size_t i = 0; i < size; ++i) {
        arr[i] = i * 0.5;  // Arbitrary values
    }
    return arr;
}

const char* concat_str_array(const char** strs, int count) {
    int total_length = 0;
    for(int i = 0; i < count; i++) {
        total_length += strlen(strs[i]);
    }
    char* result = (char*) malloc(total_length + 1);
    result[0] = '\0'; // Initialize the string
    for(int i = 0; i < count; i++) {
        strcat(result, strs[i]);
    }
    return result;
}

const char* buffer_as_return(const char* buffer, size_t size) {
    char* retval = malloc(size);
    memcpy(retval, buffer, size);
    return retval;
}


// Entry point to prevent the compiler from complaining when compiling as a shared library
// This function will not be called; it's just to satisfy the linker.
int main() {
    return 0;
}
