#include <ctype.h>
#include <stdbool.h>
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
typedef struct Point {
    int x;
    double y;
} Point;


void hexdump(const void *data, size_t size) {
    const unsigned char *byte = (const unsigned char *)data;
    size_t i, j;
    bool multiline = size > 16;
    if (multiline) printf("(Hexvalue)\nOffset\n");

    for (i = 0; i < size; i += 16) {
        if (multiline) printf("%08zx  ", i); // Offset

        // Hex bytes
        for (j = 0; j < 16; j++) {
            if (j==8) printf(" "); // Add space between the two halves of the hexdump
            if (i + j < size) {
                printf("%02x ", byte[i + j]);
            } else {
                 if (multiline) printf("   "); // Fill space if less than 16 bytes in the line
            }
        }

        if (multiline) printf(" ");
        if (!multiline) printf("= ");

        // ASCII characters
        for (j = 0; j < 16; j++) {
            if (i + j < size) {
                printf("%c", isprint(byte[i + j]) ? byte[i + j] : '.');
            }
        }

        printf("\n");
    }
}

int get_x(Point p) {
    //print a hexdump of the struct
    hexdump(&p, sizeof(Point));
    printf("p->x: %d\n", p.x);
    printf("p->y: %f\n", p.y);
    return p.x;
}

int get_x_from_structpointer(Point* p) {
    hexdump(p, sizeof(Point));
    printf("p->x: %d\n", p->x);
    printf("p->y: %f\n", p->y);
    if (p) {
        return p->x;
    }
    return 0;
}

// Function that modifies a struct
void modify_point(Point* p, int x, double y) {
    hexdump(p, sizeof(Point));
    printf("p->x: %d\n", p->x);
    printf("p->y: %f\n", p->y);
    printf("Adding %d to x and %f to y\n", x, y);
    // if (p) {
        p->x += x;
        p->y += y;

    printf("p->x: %d\n", p->x);
    printf("p->y: %f\n", p->y);
    // }
}

Point get_point(int x, double y) {
    Point p = {x,y};
    return p;
}

Point* get_p_point(int x, double y) {
    Point* p = malloc(sizeof(Point));
    p->x = x;
    p->y = y;
    return p;
}
typedef struct ComplexStruct {
    unsigned char c;
    int x;
    double y;
    unsigned char c2;
    Point p;
    int x2;
    double y2;
    Point* p2;
} ComplexStruct;

void test_complex_struct(ComplexStruct s){
    hexdump(&s, sizeof(ComplexStruct));
    printf("s.c: %c\n", s.c);
    printf("s.x: %d\n", s.x);
    printf("s.y: %f\n", s.y);
    printf("s.c2: %c\n", s.c2);
    // printf("s.p->x: %d\n", (*s.p)->x);
    // printf("s.p->y: %f\n", (*s.p)->y);
    // hexdump(*s.p, sizeof(Point));
    printf("s.p->x: %d\n", s.p.x);
    printf("s.p->y: %f\n", s.p.y);
    hexdump(&s.p, sizeof(Point));
    printf("s.x2: %d\n", s.x2);
    printf("s.y2: %f\n", s.y2);
    printf("s.p2.x: %d\n", s.p2->x);
    printf("s.p2.y: %f\n", s.p2->y);
    hexdump(s.p2, sizeof(Point));
}

void test_p_complex_struct(ComplexStruct* s){
    hexdump(&s, sizeof(ComplexStruct));
    printf("s->c: %c\n", s->c);
    printf("s->x: %d\n", s->x);
    printf("s->y: %f\n", s->y);
    printf("s->c2: %c\n", s->c2);
    // printf("s->p->x: %d\n", (*s->p)->x);
    // printf("s->p->y: %f\n", (*s->p)->y);
    // hexdump(*s->p, sizeof(Point));
    printf("s->p->x: %d\n", s->p.x);
    printf("s->p->y: %f\n", s->p.y);
    hexdump(&s->p, sizeof(Point));
    printf("s->x2: %d\n", s->x2);
    printf("s->y2: %f\n", s->y2);
    printf("s->p2.x: %d\n", s->p2->x);
    printf("s->p2.y: %f\n", s->p2->y);
    hexdump(s->p2, sizeof(Point));
}

struct larger_struct{
    int x;
    double y;
    char c;
    char* s;
};

struct struct_containing_larger_struct{
    char c;
    struct larger_struct s;
    int x;
};

struct larger_struct get_larger_struct(int x, double y, char c, const char* s){
    struct larger_struct larger_struct;
    larger_struct.x = x;
    larger_struct.y = y;
    larger_struct.c = c;
    larger_struct.s = malloc(strlen(s) + 1);
    strcpy(larger_struct.s, s);
    return larger_struct;
}

struct larger_struct* get_p_larger_struct(int x, double y, char c, const char* s){
    struct larger_struct* larger_struct_ptr = malloc(sizeof(struct larger_struct));
    larger_struct_ptr->x = x;
    larger_struct_ptr->y = y;
    larger_struct_ptr->c = c;
    larger_struct_ptr->s = malloc(strlen(s) + 1);
    strcpy(larger_struct_ptr->s, s);
    return larger_struct_ptr;
}


void test_nested_large_struct(struct struct_containing_larger_struct s){
    hexdump(&s, sizeof(struct struct_containing_larger_struct));
    printf("s.c: %c\n", s.c);
    printf("s.s.x: %d\n", s.s.x);
    printf("s.s.y: %f\n", s.s.y);
    printf("s.s.c: %c\n", s.s.c);
    printf("s.s.s: %s\n", s.s.s);
    printf("s.x: %d\n", s.x);
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
