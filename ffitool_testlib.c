#include <stdio.h>
#include <string.h>

// Simple function that adds two integers
int add(int a, int b) {
    return a + b;
}

// Function that concatenates two strings
// Note: This function returns a pointer to a static buffer for simplicity. Not thread-safe.
const char* concat(const char* a, const char* b) {
    static char result[256];
    strncpy(result, a, sizeof(result) - 1);
    strncat(result, b, sizeof(result) - strlen(result) - 1);
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

// A function that returns a static string
const char* get_message() {
    return "Hello, ffitool!";
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

// Entry point to prevent the compiler from complaining when compiling as a shared library
// This function will not be called; it's just to satisfy the linker.
int main() {
    return 0;
}
