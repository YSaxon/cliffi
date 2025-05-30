#ifndef EXCEPTION_HANDLING_H
#define EXCEPTION_HANDLING_H

#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))) && !defined(__ANDROID__)
#define use_backtrace
#endif

#ifdef use_backtrace
#include <execinfo.h>
#endif

#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(_WIN32) || defined(_WIN64)
#define sigjmp_buf jmp_buf
#define sigsetjmp(env, save) setjmp(env)
#define siglongjmp(env, val) longjmp(env, val)
#endif
#

extern bool isTestEnvExit1OnFail;

extern _Thread_local sigjmp_buf* current_exception_buffer;
extern _Thread_local char* current_exception_message;
// #ifdef use_backtrace
extern _Thread_local char** current_stacktrace_strings;
extern _Thread_local size_t current_stacktrace_size;

extern _Thread_local sigjmp_buf* old_exception_buffer;
// #endif

void raiseException(int status, char* formatstr, ...);
void printException();

#define TRY \
 do { \
    sigjmp_buf* newjmpBufferPtr = (sigjmp_buf*)malloc(sizeof(sigjmp_buf)); /* sigjmp_buf newjmpBuffer;*/ \
    old_exception_buffer = current_exception_buffer; \
    current_exception_buffer = newjmpBufferPtr; \
    /* printf("Old exception buffer: %p\n", old_exception_buffer); \
    hexdump(old_exception_buffer, sizeof(sigjmp_buf)); \
    printf("New exception buffer: %p\n", current_exception_buffer); \
    hexdump(current_exception_buffer, sizeof(sigjmp_buf)); */ \
    if (sigsetjmp(*newjmpBufferPtr, 1) == 0) {

// #define CATCH(messageSearchString) } else { \
//     /*current_exception_buffer = old_exception_buffer; */\
//     if (messageSearchString != NULL && strstr(messageSearchString, current_exception_message) == NULL) { /* this means that the catch handler shouldn't catch it*/ \
//         if (old_exception_buffer != NULL) { siglongjmp(*current_exception_buffer, 1); \ //<-- current_exception_buffer is probably wrong in that code. maybe we need to replace it with old_exception_buffer? \
//         } else { \
//             fprintf(stderr, "Not able to rethrow exception.\n"); \
//             goto handleException; \
//         } \
//     } else { \
//         handleException: \
//         current_exception_buffer = old_exception_buffer;

// #define CATCHALL CATCH(NULL)

#define CATCHALL } else { { /*handleException:*/ \
    current_exception_buffer = old_exception_buffer; \

// #if defined (use_backtrace)
#define freebacktrace free(current_stacktrace_strings); current_stacktrace_strings = NULL; current_stacktrace_size = 0;
// #else
// #define freebacktrace
// #endif

#define END_TRY }} \
    free(newjmpBufferPtr); \
    current_exception_buffer = old_exception_buffer; \
    if (current_exception_message != NULL) { \
        free(current_exception_message); \
        current_exception_message = NULL; \
    } \
    freebacktrace \
} while (0);


void raiseException(int status, char* formatstr, ...);
void setCodeSectionForSegfaultHandler(const char* section);
void unsetCodeSectionForSegfaultHandler();

void main_method_install_exception_handlers();

#endif // EXCEPTION_HANDLING_H

