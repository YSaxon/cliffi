#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <stddef.h>

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
int increment_at_pointer(int* a) { //deliberately unsafe
    (*a)++;
    return *a;
}

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
DWORD WINAPI do_segfault(LPVOID lpParam) {
    int* p = NULL;
    *p = 42;
    return 0;
}
#else
#include <pthread.h>
void* do_segfault(void* arg) {
    int* volatile p = NULL;
    increment_at_pointer(p);
    // *p = 42;
    return NULL;
}
#endif

// void do_double_free() { // unused
//     int* p = malloc(sizeof(int));
//     free(p);
//     free(p);
// }

void do_buffer_overflow() {
    char buffer[10];
    // mark it to override warnings
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Warray-bounds"
    buffer[15] = 'a';
    #pragma clang diagnostic pop
}

void do_segfault_in_another_thread() {
    //start another thread that does a segfault
    #if !defined(_WIN32) && !defined(_WIN64)
    pthread_t thread;
    pthread_create(&thread, NULL, (void* (*)(void*))do_segfault, NULL);
    pthread_join(thread, NULL);
    #else
    #include <windows.h>
    HANDLE hThread; // Variable to store the thread handle
    DWORD threadId; // Variable to store the thread ID (optional, CreateThread can return it)
    hThread = CreateThread(
        NULL,        // Default security attributes
        0,           // Default stack size
        (LPTHREAD_START_ROUTINE)do_segfault, // Thread function
        NULL,        // Argument to thread function
        0,           // Default creation flags
        &threadId);  // Pointer to receive thread ID (can be NULL if not needed)
    if (hThread == NULL) {
        fprintf(stderr, "Error creating thread: %lu\n", GetLastError());
        return;
    }
    // Wait for the thread to finish
    WaitForSingleObject(hThread, INFINITE);
    // Close the thread handle
    CloseHandle(hThread);
    #endif
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
    return "Hello, cliffi!"; // this maybe shows us we need to handle string literals in data?
}

void* getAddressOfGetMessage() {
    return &get_message;
}


// Struct for demonstration
typedef struct Point {
    int x;
    double y;
} Point;

void hexdump(const void* data, size_t size) {
    const unsigned char* byte = (const unsigned char*)data;
    size_t i, j;
    bool multiline = size > 16;
    if (multiline) printf("(Hexvalue)\nOffset\n");

    for (i = 0; i < size; i += 16) {
        if (multiline) printf("%08zx  ", i); // Offset

        // Hex bytes
        for (j = 0; j < 16; j++) {
            if (j == 8) printf(" "); // Add space between the two halves of the hexdump
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
    // print a hexdump of the struct
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
    Point p = {x, y};
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

void test_complex_struct(ComplexStruct s) {
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

void test_p_complex_struct(ComplexStruct* s) {
    hexdump(s, sizeof(ComplexStruct));
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

ComplexStruct get_complex_struct(unsigned char c, int x, double y, unsigned char c2, int x2, double y2, int x3, double y3, int x4, double y4) {
    ComplexStruct s = {};
    // typedef struct ComplexStruct {
    //     unsigned char c;
    //     int x;
    //     double y;
    //     unsigned char c2;
    //     Point p;
    //     int x2;
    //     double y2;
    //     Point* p2;
    // } ComplexStruct;
    s.c = c;
    s.x = x;
    s.y = y;
    s.c2 = c2;
    s.p = get_point(x2, y2);
    s.x2 = x3;
    s.y2 = y3;
    s.p2 = get_p_point(x4, y4);
    return s;
}

ComplexStruct* get_p_complex_struct(unsigned char c, int x, double y, unsigned char c2, int x2, double y2, int x3, double y3, int x4, double y4) {
    ComplexStruct* s = malloc(sizeof(ComplexStruct));
    s->c = c;
    s->x = x;
    s->y = y;
    s->c2 = c2;
    s->p = get_point(x2, y2);
    s->x2 = x3;
    s->y2 = y3;
    s->p2 = get_p_point(x4, y4);
    return s;
}

struct larger_struct {
    int x;
    double y;
    char c;
    char* s;
};

struct struct_containing_larger_struct {
    char c;
    struct larger_struct s;
    int x;
};

struct larger_struct get_larger_struct(int x, double y, char c, const char* s) {
    struct larger_struct larger_struct;
    larger_struct.x = x;
    larger_struct.y = y;
    larger_struct.c = c;
    larger_struct.s = malloc(strlen(s) + 1);
    strcpy(larger_struct.s, s);
    return larger_struct;
}

struct larger_struct* get_p_larger_struct(int x, double y, char c, const char* s) {
    struct larger_struct* larger_struct_ptr = malloc(sizeof(struct larger_struct));
    larger_struct_ptr->x = x;
    larger_struct_ptr->y = y;
    larger_struct_ptr->c = c;
    larger_struct_ptr->s = malloc(strlen(s) + 1);
    strcpy(larger_struct_ptr->s, s);
    return larger_struct_ptr;
}

void test_nested_large_struct(struct struct_containing_larger_struct s) {
    hexdump(&s, sizeof(struct struct_containing_larger_struct));
    printf("s.c: %c\n", s.c);
    printf("s.s.x: %d\n", s.s.x);
    printf("s.s.y: %f\n", s.s.y);
    printf("s.s.c: %c\n", s.s.c);
    printf("s.s.s: %s\n", s.s.s);
    printf("s.x: %d\n", s.x);
}

struct struct_containing_larger_struct get_nested_larger_struct(char c, int x, double y, char c2, const char* s, int x2) {
    struct struct_containing_larger_struct my_struct = {};
    my_struct.c = c;
    my_struct.s.x = x;
    my_struct.s.y = y;
    my_struct.s.c = c2;
    my_struct.s.s = strdup(s);
    my_struct.x = x2;
    return my_struct;
}

struct struct_containing_larger_struct* get_p_nested_larger_struct(char c, int x, double y, char c2, const char* s, int x2) {
    struct struct_containing_larger_struct* my_struct = malloc(sizeof(struct struct_containing_larger_struct));
    my_struct->c = c;
    my_struct->s.x = x;
    my_struct->s.y = y;
    my_struct->s.c = c2;
    my_struct->s.s = strdup(s);
    my_struct->x = x2;
    return my_struct;
}

void test_fixed_buffer_10_chars_arg_then_double_arg(char* arr, double d) {
    hexdump(arr, 10);
    printf("s: %s\n", arr);
    printf("d: %f\n", d);
}

struct simple_struct_embedded_array_10_chars_then_double {
    char s[10];
    double d;
};

void test_simple_struct_embedded_array_10_chars_then_double(struct simple_struct_embedded_array_10_chars_then_double s) {
    hexdump(&s, sizeof(struct simple_struct_embedded_array_10_chars_then_double));
    printf("s.s: %s\n", s.s);
    printf("s.d: %f\n", s.d);
}

struct even_simpler_struct_embedded_array_10_chars {
    char s[10];
};

void test_even_simpler_struct_embedded_array_10_chars(struct even_simpler_struct_embedded_array_10_chars s) {
    hexdump(&s, sizeof(struct even_simpler_struct_embedded_array_10_chars));
    printf("s.s: %s\n", s.s);
}

struct struct_with_embedded_array_pointer {
    char (*s)[10];
};

void test_struct_with_char_pointer(struct struct_with_embedded_array_pointer s) {
    hexdump(&s, sizeof(struct struct_with_embedded_array_pointer));
    printf("s: %s\n", *s.s);
    for (int i = 0; i < 10; i++) { // 10th element is null terminator
        printf("s.s[%d]: %c\n", i, (*s.s)[i]);
    }
}

struct struct_with_embedded_array_pointer return_struct_with_embedded_array_pointer() {
    struct struct_with_embedded_array_pointer s = {};
    s.s = malloc(10);
    strcpy(*s.s, "hello");
    return s;
}

void test_p_struct_with_char_pointer(struct struct_with_embedded_array_pointer* s) {
    hexdump(s, sizeof(struct struct_with_embedded_array_pointer));
    hexdump(s->s, 10);
    printf("s: %s\n", *s->s);
    for (int i = 0; i < 10; i++) { // 10th element is null terminator
        printf("s.s[%d]: %c\n", i, (*s->s)[i]);
    }
}

struct struct_with_embedded_array_pointer* return_p_struct_with_embedded_array_pointer() {
    struct struct_with_embedded_array_pointer* s = malloc(sizeof(struct struct_with_embedded_array_pointer));
    s->s = malloc(10);
    strcpy(*s->s, "hello");
    return s;
}

struct struct_with_embedded_array {
    int x;
    char s[10];
    int a[3];
    int y;
};

struct struct_with_embedded_array_intonly {
    int x;
    int a[3];
    int y;
};

struct struct_with_embedded_array_charonly {
    int x;
    char s[10];
    int y;
};

void test_struct_with_embedded_array(struct struct_with_embedded_array s) {
    hexdump(&s, sizeof(struct struct_with_embedded_array));
    printf("s.x: %d\n", s.x);
    printf("s.s: %s\n", s.s);
    hexdump(s.s, 10);
    printf("s.a[0]: %d\n", s.a[0]);
    printf("s.a[1]: %d\n", s.a[1]);
    printf("s.a[2]: %d\n", s.a[2]);
    printf("s.y: %d\n", s.y);
}

void test_p_struct_with_embedded_array(struct struct_with_embedded_array* s) {
    hexdump(s, sizeof(struct struct_with_embedded_array));
    printf("s->x: %d\n", s->x);
    printf("s->s: %s\n", s->s);
    hexdump(s->s, 10);
    printf("s->a[0]: %d\n", s->a[0]);
    printf("s->a[1]: %d\n", s->a[1]);
    printf("s->a[2]: %d\n", s->a[2]);
    printf("s->y: %d\n", s->y);
}

void test_p_struct_with_embedded_array_intonly(struct struct_with_embedded_array_intonly* s) {
    hexdump(s, sizeof(struct struct_with_embedded_array_intonly));
    printf("s->x: %d\n", s->x);
    printf("s->a[0]: %d\n", s->a[0]);
    printf("s->a[1]: %d\n", s->a[1]);
    printf("s->a[2]: %d\n", s->a[2]);
    printf("s->y: %d\n", s->y);
}

struct struct_with_embedded_array_intonly test_p_struct_with_embedded_array_intonly_return_struct(struct struct_with_embedded_array_intonly* s) {
    hexdump(s, sizeof(struct struct_with_embedded_array_intonly));
    printf("s->x: %d\n", s->x);
    printf("s->a[0]: %d\n", s->a[0]);
    printf("s->a[1]: %d\n", s->a[1]);
    printf("s->a[2]: %d\n", s->a[2]);
    printf("s->y: %d\n", s->y);
    return *s;
}

struct struct_with_embedded_array_intonly return_struct_with_embedded_array_intonly() {
    struct struct_with_embedded_array_intonly s = {};
    s.x = 1;
    s.a[0] = 2;
    s.a[1] = 3;
    s.a[2] = 4;
    s.y = 5;
    return s;
}

struct simple_struct_embedded_array {
    int ints[3];
};

struct simple_struct_embedded_array return_simple_struct_embedded_array() {
    struct simple_struct_embedded_array s = {};
    s.ints[0] = 1;
    s.ints[1] = 2;
    s.ints[2] = 3;
    return s;
}

struct struct_with_embedded_array_intonly* test_p_struct_with_embedded_array_intonly_return_p_struct(struct struct_with_embedded_array_intonly* s) {
    hexdump(s, sizeof(struct struct_with_embedded_array_intonly));
    printf("s->x: %d\n", s->x);
    printf("s->a[0]: %d\n", s->a[0]);
    printf("s->a[1]: %d\n", s->a[1]);
    printf("s->a[2]: %d\n", s->a[2]);
    printf("s->y: %d\n", s->y);
    return s;
}

void test_p_struct_with_embedded_array_charonly(struct struct_with_embedded_array_charonly* s) {
    hexdump(s, sizeof(struct struct_with_embedded_array_charonly));
    printf("s->x: %d\n", s->x);
    printf("s->s: %s\n", s->s);
    printf("s->y: %d\n", s->y);
}

void test_p_struct_with_embedded_array_changed(struct struct_with_embedded_array* s) {
    hexdump(s, sizeof(struct struct_with_embedded_array));
    s->x = 5;
    strcpy(s->s, "changed");
    s->a[0] = 10;
    s->a[1] = 11;
    s->a[2] = 12;
    s->y = 15;
}

void test_struct_with_embedded_array_changed(struct struct_with_embedded_array s) {
    hexdump(&s, sizeof(struct struct_with_embedded_array));
    s.x = 5;
    strcpy(s.s, "changed");
    s.a[0] = 10;
    s.a[1] = 11;
    s.a[2] = 12;
    s.y = 15;
}

struct struct_with_embedded_array return_struct_with_embedded_array() {
    struct struct_with_embedded_array s = {};
    s.x = 1;
    strcpy(s.s, "hello");
    s.a[0] = 2;
    s.a[1] = 3;
    s.a[2] = 4;
    s.y = 5;
    return s;
}


struct embedded_with_pointer {
    int* x;
    char* s;
};

struct parent_with_embedded_pointer {
    char x;
    struct embedded_with_pointer p;
    struct embedded_with_pointer* p2;
    int y;
};

void test_parent_with_embedded_pointer(struct parent_with_embedded_pointer s) {
    hexdump(&s, sizeof(struct parent_with_embedded_pointer));
    printf("s.p.x: %d\n", *s.p.x);
    printf("s.p.s: %s\n", s.p.s);
    printf("s.p2->x: %d\n", *s.p2->x);
    printf("s.p2->s: %s\n", s.p2->s);
    s.p.x[0]++;
    //capitalize the string
    for (int i = 0; i < strlen(s.p.s); i++) {
        s.p.s[i] = toupper(s.p.s[i]);
    }
    s.p2->x[0]++;
    //capitalize the string
    for (int i = 0; i < strlen(s.p2->s); i++) {
        s.p2->s[i] = toupper(s.p2->s[i]);
    }
}

struct simpler_parent_with_embedded_pointer {
    char x;
    struct embedded_with_pointer p;
};

void simple_test_parent_with_embedded_pointer(struct simpler_parent_with_embedded_pointer s) {
    hexdump(&s, sizeof(struct parent_with_embedded_pointer));
    printf("s.p.x: %d\n", *s.p.x);
    printf("s.p.s: %s\n", s.p.s);
    s.p.x[0]++;
    //capitalize the string
    for (int i = 0; i < strlen(s.p.s); i++) {
        s.p.s[i] = toupper(s.p.s[i]);
    }
}

struct simpler_parent_with_pointer_struct_embedded_pointer {
    char x;
    struct embedded_with_pointer *p;
};

void simple_test_parent_with_pointer_struct_embedded_pointer(struct simpler_parent_with_pointer_struct_embedded_pointer s) {
    hexdump(&s, sizeof(struct simpler_parent_with_pointer_struct_embedded_pointer));
    printf("s.x: %c\n", s.x);
    printf("s.p: %p\n", s.p);
    hexdump(s.p, sizeof(struct embedded_with_pointer));
    printf("s.p->x: %d\n", *s.p->x);
    printf("s.p->s: %s\n", s.p->s);
    (*s.p->x)++;
    //capitalize the string
    for (int i = 0; i < strlen(s.p->s); i++) {
        s.p->s[i] = toupper(s.p->s[i]);
    }
}

void simple_test_int_pointer(int* x) {
    hexdump(x, sizeof(int));
    printf("x: %d\n", *x);
    (*x)++;
}

struct with_int_pointer {
    int* x;
};

void test_struct_with_int_pointer(struct with_int_pointer s) {
    hexdump(&s, sizeof(struct with_int_pointer));
    printf("s.x: %d\n", *s.x);
    (*s.x)++;
}

// Function that takes an array of integers and returns the sum
int sum_array(int* arr, int size) {
    int sum = 0;
    for (int i = 0; i < size; i++) {
        sum += arr[i];
    }
    return sum;
}

// Function that demonstrates deeper pointer depths
int increment_at_pointer_pointer(int** pp) {
    if (pp && *pp) {
        (**pp)++;
        return **pp;
    }
    return -1;
}

// Function returning a dynamically allocated array of doubles
double* get_array_of_doubles(size_t size) {
    double* arr = (double*)malloc(size * sizeof(double));
    for (size_t i = 0; i < size; ++i) {
        arr[i] = i * 0.5; // Arbitrary values
    }
    return arr;
}

const char* concat_str_array(const char** strs, int count) {
    int total_length = 0;
    for (int i = 0; i < count; i++) {
        total_length += strlen(strs[i]);
    }
    char* result = (char*)malloc(total_length + 1);
    result[0] = '\0'; // Initialize the string
    for (int i = 0; i < count; i++) {
        strncat(result, strs[i], total_length);
    }
    return result;
}

const char* buffer_as_return(const char* buffer, size_t size) {
    char* retval = malloc(size);
    memcpy(retval, buffer, size);
    return retval;
}

struct __attribute__((packed)) PackedStruct {
    char c;
    int i;
    char s[2];
    double d;
    int a[3];
    char c2;
};

struct UnPackedStruct {
    char c;
    int i;
    char s[2];
    double d;
    int a[3];
    char c2;
};

struct PackedStruct get_packed_struct() {
    struct PackedStruct s = {};
    s.c = 'a';
    s.i = 42;
    s.s[0] = 'b';
    s.s[1] = 'c';
    s.d = 3.14;
    s.a[0] = 1;
    s.a[1] = 2;
    s.a[2] = 3;
    s.c2 = 'd';
    return s;
}

struct UnPackedStruct get_unpacked_struct() {
    struct UnPackedStruct s = {};
    s.c = 'a';
    s.i = 42;
    s.s[0] = 'b';
    s.s[1] = 'c';
    s.d = 3.14;
    s.a[0] = 1;
    s.a[1] = 2;
    s.a[2] = 3;
    s.c2 = 'd';
    return s;
}

// called as v -SK: c i ac2 d ai3 c :S

struct PackedStruct* get_p_packed_struct() {
    struct PackedStruct* s = malloc(sizeof(struct PackedStruct));
    s->c = 'a';
    s->i = 42;
    s->s[0] = 'b';
    s->s[1] = 'c';
    s->d = 3.14;
    s->a[0] = 1;
    s->a[1] = 2;
    s->a[2] = 3;
    s->c2 = 'd';
    return s;
}

void test_packed_struct(struct PackedStruct s) {
    printf("sizeof(struct PackedStruct): %zu\n", sizeof(struct PackedStruct));
    hexdump(&s, sizeof(struct PackedStruct));
    printf("s.c: %c\n", s.c);
    printf("s.i: %d\n", s.i);
    printf("s.s[0]: %c\n", s.s[0]);
    printf("s.s[1]: %c\n", s.s[1]);
    printf("s.d: %f\n", s.d);
    printf("s.a: %d %d %d\n", s.a[0], s.a[1], s.a[2]);
    printf("s.c2: %c\n", s.c2);
}

void test_p_packed_struct(struct PackedStruct* s) {
    hexdump(s, sizeof(struct PackedStruct));
    printf("s->c: %c\n", s->c);
    printf("s->i: %d\n", s->i);
    printf("s->s: %c%c\n", s->s[0], s->s[1]);
    printf("s->d: %f\n", s->d);
    printf("s->a: %d %d %d\n", s->a[0], s->a[1], s->a[2]);
    printf("s->c2: %c\n", s->c2);
}

void test_unpacked_struct(struct UnPackedStruct s) {
    hexdump(&s, sizeof(struct UnPackedStruct));
    printf("s.c: %c\n", s.c);
    printf("s.i: %d\n", s.i);
    printf("s.s[0]: %c\n", s.s[0]);
    printf("s.s[1]: %c\n", s.s[1]);
    printf("s.d: %f\n", s.d);
    printf("s.a: %d %d %d\n", s.a[0], s.a[1], s.a[2]);
    printf("s.c2: %c\n", s.c2);
}

int sum_func_with_int_varargs(int count, ...) {
    va_list args;
    int sum = 0;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        int arg = va_arg(args, int);
        sum += arg;
        printf("got vararg %d: %d\n", i, arg);
    }
    va_end(args);
    return sum;
}

void my_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void varargs_structs(int count, ...) {
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        Point p = va_arg(args, Point);
        printf("got vararg %d: %d, %f\n", i, p.x, p.y);
    }
    va_end(args);
}

void varargs_p_structs(int count, ...) {
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        Point* p = va_arg(args, Point*);
        printf("got vararg %d: %d, %f\n", i, p->x, p->y);
    }
    va_end(args);
}

char get_char() {
    return 'a';
}

short get_short() {
    return 1;
}

short get_short_negative5() {
    return -5;
}

unsigned char get_uchar_255() {
    return 255;
}

unsigned short get_ushort_65535() {
    return 65535;
}

float get_float() {
    return 1.5;
}

void return_some_numbers(int** numbers, int* count) {
    *count = 3;
    *numbers = malloc(3 * sizeof(int));
    (*numbers)[0] = 1;
    (*numbers)[1] = 2;
    (*numbers)[2] = 3;
    return;
}

long add_long(long a, long b) {
    return a + b;
}

unsigned long add_ulong(unsigned long a, unsigned long b) {
    return a + b;
}

struct has_array_of_int_pointers {
    int* arr[3];
};

int test_array_of_pointers_in_struct(struct has_array_of_int_pointers s) {
    int sum = 0;
    for (int i = 0; i < 3; i++) {
        sum += *s.arr[i];
    }
    return sum;
}

int test_array_of_int_pointers_simple(int* arr[3]) {
    int sum = 0;
    for (int i = 0; i < 3; i++) {
        sum += *arr[i];
    }
    return sum;
}
// int** grand_test_of_arrays_of_pointers(struct has_array_of_int_pointers s, struct has_array_of_int_pointers* s2, int arr[3], int (*arr2)[3]);

int** grand_test_of_arrays_of_int_pointers(struct has_array_of_int_pointers s, struct has_array_of_int_pointers* s2, int* arr[3], int* (**arr2)[3]) {
    int** return_ints = malloc(12 * sizeof(int*));
    for (int i = 0; i < 3; i++) {
        printf("s.arr[%d]: %d\n", i, *s.arr[i]);
        return_ints[i] = s.arr[i];
    }
    for (int i = 0; i < 3; i++) {
        printf("s2->arr[%d]: %d\n", i, *s2->arr[i]);
        return_ints[i + 3] = s2->arr[i];
    }
    for (int i = 0; i < 3; i++) {
        printf("arr[%d]: %d\n", i, *arr[i]);
        return_ints[i + 6] = arr[i];
    }
    for (int i = 0; i < 3; i++) {
        printf("arr2[%d]: %d\n", i, *(**arr2)[i]);
        return_ints[i + 9] = (**arr2)[i];
    }
    return return_ints;
}

int global_int = 0;
int increment_global() {
    return ++global_int;
}

void* get_address_of_global() {
    return &global_int;
}

char global_buffer1[500] = {0};
char global_buffer2[500] = {0};

void* get_address_of_global_buffer1() {
    return global_buffer1;
}
void* get_address_of_global_buffer2() {
    return global_buffer2;
}

char global_string[200] = "Hello, world!";
char* get_global_string() {
    return global_string;
}

char (*global_string_ptr)[200] = &global_string;

char (*get_global_string_ptr())[200] {
    return global_string_ptr;
}

bool is_null(void* ptr) {
    return ptr == NULL;
}
bool is_pointer_to_null(void** ptr) {
    return *ptr == NULL;
}

bool return_bool(bool value) {
    return value;
}

bool check_bool(bool a, bool b, bool c) {
    printf("Got values: %s, %s, %s\n",
           a ? "true" : "false",
           b ? "true" : "false",
           c ? "true" : "false");
    return a && b && c;
}

bool* return_bool_array(bool values[3]) {
    bool* result = malloc(3 * sizeof(bool));
    memcpy(result, values, 3 * sizeof(bool));
    return result;
}

struct bool_struct {
    bool flag;
    int value;
};

struct bool_struct create_bool_struct(bool flag, int value) {
    struct bool_struct result = {flag, value};
    return result;
}

// Test functions for bool type
bool test_bool_true(bool val) {
    return val == true;
}

bool test_bool_false(bool val) {
    return val == false;
}

bool test_bool_array(bool arr[3]) {
    return arr[0] == true && arr[1] == false && arr[2] == true;
}

bool test_bool_return(void) {
    return true;
}

bool is_equal(int a, int b){
    return a==b;
}

// Entry point to prevent the compiler from complaining when compiling as a shared library
// This function will not be called; it's just to satisfy the linker.
int main() {
    return 0;
}
