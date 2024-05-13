
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    #define pthread_t DWORD
    #define pthread_self GetCurrentThreadId
    #define pthread_equal(a, b) (a == b)
    #define pthread_exit ExitThread
#else
    #include <pthread.h>
#endif
#include <setjmp.h>
#include <signal.h>
#include "shims.h"
#include "exception_handling.h"


bool isTestEnvExit1OnFail = false;

sigjmp_buf rootJmpBuffer;

_Thread_local sigjmp_buf* current_exception_buffer = &rootJmpBuffer;
_Thread_local char* current_exception_message = NULL;
_Thread_local char** current_stacktrace_strings = NULL;
_Thread_local size_t current_stacktrace_size = 0;

#ifdef use_backtrace
void saveStackTrace() {
    void* array[10];
    size_t i;

    if (current_stacktrace_strings == NULL) {

        current_stacktrace_size = backtrace(array, 10);
        if (current_stacktrace_size == 0) {
            // current_stacktrace_size = 1;
            // current_stacktrace_strings = malloc(sizeof(char*));
            // current_stacktrace_strings[0] = strdup("(no stack trace available)");
            return;
        }

        current_stacktrace_strings = backtrace_symbols(array, current_stacktrace_size);

    } else {

        if (current_exception_message == NULL) {
            printf("While handling exception, current_stacktrace exists but current_exception_message does not. We may have called saveStackTrace twice by mistake\n");
            current_exception_message = strdup("(null message)");
            }

        size_t new_size = backtrace(array, 10);
        char** new_strings = backtrace_symbols(array, current_stacktrace_size);

        int lines_for_subheader = 1;

        size_t combined_size = current_stacktrace_size + lines_for_subheader + new_size;
        char** combined_strings = malloc(combined_size * sizeof(char*));

        // Add the new stack trace first
        for (i = 0; i < new_size; i++) {
            combined_strings[i] = new_strings[i];
        }

        // Add the subheader
        char* whileHandling = malloc(strlen("\n\tWhile handling exception: \n") + strlen(current_exception_message) + 1);
        strcpy(whileHandling, "\n\tWhile handling exception: ");
        strcat(whileHandling, current_exception_message);
        combined_strings[new_size] = whileHandling;

        // Add the original stack trace after the subheader
        for (i = 0; i < current_stacktrace_size; i++) {
            combined_strings[new_size + lines_for_subheader + i] = current_stacktrace_strings[i];
        }

        free(current_stacktrace_strings);
        free(new_strings);
        current_stacktrace_strings = combined_strings;
        current_stacktrace_size = combined_size;
    }
}

void printStackTrace(){
    if (current_stacktrace_size == 0) {
        fprintf(stderr, "No stack trace available\n");
        return;
    }
    fprintf(stderr, "Stack trace:\n");
    for (size_t i = 0; i < current_stacktrace_size; i++) {
        fprintf(stderr, "%s\n", current_stacktrace_strings[i]);
    }
    free(current_stacktrace_strings);
    current_stacktrace_strings = NULL;
    current_stacktrace_size = 0;
}
#else
    #just save the previous error message(s) in a buffer in order to print "while handling exception: " in the catch block
    void saveStackTrace() {
        if (current_exception_message == NULL) return;
        // put the existing message in the stack trace buffer and free it for the new message
        if (current_stacktrace_strings == NULL) {
            current_stacktrace_strings = malloc(sizeof(char*));
            current_stacktrace_strings[0] = current_exception_message;
            current_stacktrace_size = 1;
        } else {
            size_t combined_size = current_stacktrace_size + 1;
            char** combined_strings = malloc(combined_size * sizeof(char*));
            for (size_t i = 0; i < current_stacktrace_size; i++)
                combined_strings[i] = current_stacktrace_strings[i];
            combined_strings[current_stacktrace_size] = current_exception_message;
            free(current_stacktrace_strings);
            current_stacktrace_strings = combined_strings;
            current_stacktrace_size = combined_size;
        }

    }
    void printStackTrace() {
        for (size_t i = 0; i < current_stacktrace_size; i++) {
            fprintf(stderr, "While handling exception: %s\n", current_stacktrace_strings[i]);

    }

#endif


void raiseException(int status, char* formatstr, ...) {
#ifdef use_backtrace
    saveStackTrace(); // call it first so that the stack trace is saved with the old message before the message is overwritten
#endif
    if (current_exception_message != NULL) {
        free(current_exception_message);
        current_exception_message = NULL;
    }
    if (formatstr != NULL) {
        va_list args;
        va_start(args, formatstr);
        vasprintf(&current_exception_message, formatstr, args);
        va_end(args);
    }
    siglongjmp(*current_exception_buffer, status);
}

void printException() {
    if (current_exception_message == NULL || strlen(current_exception_message) == 0){
        fprintf(stderr, "Error thrown with no message\n");
    } else {
        fprintf(stderr, "%s\n", current_exception_message);
        free(current_exception_message);
        current_exception_message = NULL;
    }
#ifdef use_backtrace
    printStackTrace();
#endif
}

#define SEGFAULT_SECTION_UNSET "(unset)";
_Thread_local const char* SEGFAULT_SECTION = SEGFAULT_SECTION_UNSET;

void setCodeSectionForSegfaultHandler(const char* section) {
    SEGFAULT_SECTION = section;
}
void unsetCodeSectionForSegfaultHandler() {
    SEGFAULT_SECTION = SEGFAULT_SECTION_UNSET;
}



pthread_t main_thread_id;
void set_main_threadid() {
    main_thread_id = pthread_self();
}

bool is_main_thread() {
    return pthread_equal(main_thread_id, pthread_self());
}

void terminateThread() {
    pthread_exit(NULL);
}

void handleSegfault(int signal) {
    if (!is_main_thread()) {
        fprintf(stderr, "Caught segfault on non-main thread. Terminating thread.\n");
        terminateThread();
    }
    if (current_exception_message != NULL) {
        printf("Freeing current_exception_message which was unexpectedly not null: %s\n", current_exception_message);
        free(current_exception_message);
        current_exception_message = NULL;
    }
    current_exception_message = malloc(strlen("Segmentation fault in section: ") + strlen(SEGFAULT_SECTION) + 1);
    strcpy(current_exception_message, "Segmentation fault in section: ");
    strcat(current_exception_message, SEGFAULT_SECTION);
    // vasprintf(&current_exception_message, "Segmentation fault in section: %s", (char*)SEGFAULT_SECTION);
    siglongjmp(*current_exception_buffer, 1);
}





void install_root_exception_handler() {
    set_main_threadid();
    if (sigsetjmp(rootJmpBuffer, 1) != 0) {
        if (is_main_thread()){
            fprintf(stderr,"Root exception handler caught an exception on main thread\n");
            printException();
            exit(1);
        }
        else {
            fprintf(stderr, "Caught exception on non-main thread\n");
            printException();
        }
    }
}
void install_segfault_handler() {
    signal(SIGSEGV, handleSegfault);
}

void main_method_install_exception_handlers() {
    install_root_exception_handler();
    install_segfault_handler();
}
