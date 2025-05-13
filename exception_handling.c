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
    #define pthread_exit(a) ExitThread(0)
#else
    #include <pthread.h>
#endif
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
 /*x86 or risc based systems*/
#if (defined(__i386__) || defined(__x86_64__) || defined(__riscv)) && !defined(_WIN32)
    #include <ucontext.h>
#endif
#include "shims.h"
#include "exception_handling.h"


bool isTestEnvExit1OnFail = false;

sigjmp_buf rootJmpBuffer;

_Thread_local sigjmp_buf* current_exception_buffer = &rootJmpBuffer;
_Thread_local sigjmp_buf* old_exception_buffer;
_Thread_local char* current_exception_message = NULL;
_Thread_local char** current_stacktrace_strings = NULL;
_Thread_local size_t current_stacktrace_size = 0;

#ifdef use_backtrace
void saveStackTrace() {
    fprintf(stderr, "[TRACE] %s: Entered\n", __func__);
    void* array[10];
    size_t i;

    if (current_stacktrace_strings == NULL) {
        fprintf(stderr, "[TRACE] %s: current_stacktrace_strings is NULL\n", __func__);

        current_stacktrace_size = backtrace(array, 10);
        fprintf(stderr, "[TRACE] %s: backtrace returned size %zu\n", __func__, current_stacktrace_size);
        if (current_stacktrace_size == 0) {
            fprintf(stderr, "[TRACE] %s: backtrace size is 0, returning\n", __func__);
            // current_stacktrace_size = 1;
            // current_stacktrace_strings = malloc(sizeof(char*));
            // current_stacktrace_strings[0] = strdup("(no stack trace available)");
            return;
        }

        current_stacktrace_strings = backtrace_symbols(array, current_stacktrace_size);
        fprintf(stderr, "[TRACE] %s: backtrace_symbols returned %p\n", __func__, (void*)current_stacktrace_strings);

    } else {
        fprintf(stderr, "[TRACE] %s: current_stacktrace_strings is not NULL\n", __func__);

        if (current_exception_message == NULL) {
            printf("While handling exception, current_stacktrace exists but current_exception_message does not. We may have called saveStackTrace twice by mistake\n");
            current_exception_message = strdup("(null message)");
            fprintf(stderr, "[TRACE] %s: current_exception_message was NULL, set to '%s'\n", __func__, current_exception_message);
        }

        size_t new_size = backtrace(array, 10);
        fprintf(stderr, "[TRACE] %s: second backtrace returned size %zu\n", __func__, new_size);
        char** new_strings = backtrace_symbols(array, current_stacktrace_size);
        fprintf(stderr, "[TRACE] %s: second backtrace_symbols returned %p\n", __func__, (void*)new_strings);

        int lines_for_subheader = 1;

        size_t combined_size = current_stacktrace_size + lines_for_subheader + new_size;
        fprintf(stderr, "[TRACE] %s: combined_size = %zu (old_size=%zu + subheader=%d + new_size=%zu)\n", __func__, combined_size, current_stacktrace_size, lines_for_subheader, new_size);
        char** combined_strings = malloc(combined_size * sizeof(char*));
        fprintf(stderr, "[TRACE] %s: allocated combined_strings %p\n", __func__, (void*)combined_strings);

        // Add the new stack trace first
        fprintf(stderr, "[TRACE] %s: Adding new stack trace (%zu lines)\n", __func__, new_size);
        for (i = 0; i < new_size; i++) {
            combined_strings[i] = new_strings[i];
        }

        // Add the subheader
        fprintf(stderr, "[TRACE] %s: Adding subheader '%s'\n", __func__, current_exception_message);
        char* whileHandling = malloc(strlen("\n\tWhile handling exception: \n") + strlen(current_exception_message) + 1);
        strcpy(whileHandling, "\n\tWhile handling exception: ");
        strcat(whileHandling, current_exception_message);
        combined_strings[new_size] = whileHandling;
        fprintf(stderr, "[TRACE] %s: Created subheader string '%s' at %p\n", __func__, whileHandling, (void*)whileHandling);


        // Add the original stack trace after the subheader
        fprintf(stderr, "[TRACE] %s: Adding original stack trace (%zu lines)\n", __func__, current_stacktrace_size);
        for (i = 0; i < current_stacktrace_size; i++) {
            combined_strings[new_size + lines_for_subheader + i] = current_stacktrace_strings[i];
        }

        fprintf(stderr, "[TRACE] %s: Freeing old current_stacktrace_strings %p\n", __func__, (void*)current_stacktrace_strings);
        free(current_stacktrace_strings);
        fprintf(stderr, "[TRACE] %s: Freeing new_strings %p\n", __func__, (void*)new_strings);
        free(new_strings);
        current_stacktrace_strings = combined_strings;
        current_stacktrace_size = combined_size;
        fprintf(stderr, "[TRACE] %s: Updated current_stacktrace_strings to %p and size to %zu\n", __func__, (void*)current_stacktrace_strings, current_stacktrace_size);
    }
    fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
}

void printStackTrace(){
    fprintf(stderr, "[TRACE] %s: Entered\n", __func__);
    if (current_stacktrace_size == 0) {
        fprintf(stderr, "[TRACE] %s: current_stacktrace_size is 0\n", __func__);
        fprintf(stderr, "No stack trace available\n");
        fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
        return;
    }
    fprintf(stderr, "[TRACE] %s: Printing stack trace of size %zu\n", __func__, current_stacktrace_size);
    fprintf(stderr, "Stack trace:\n");
    for (size_t i = 0; i < current_stacktrace_size; i++) {
        fprintf(stderr, "[TRACE] %s: Printing line %zu: '%s'\n", __func__, i, current_stacktrace_strings[i]);
        fprintf(stderr, "%s\n", current_stacktrace_strings[i]);
    }
    fprintf(stderr, "[TRACE] %s: Freeing current_stacktrace_strings %p\n", __func__, (void*)current_stacktrace_strings);
    free(current_stacktrace_strings);
    current_stacktrace_strings = NULL;
    current_stacktrace_size = 0;
    fprintf(stderr, "[TRACE] %s: Reset current_stacktrace_strings to NULL and size to 0\n", __func__);
    fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
}
#else
    //just save the previous error message(s) in a buffer in order to print "while handling exception: " in the catch block
    void saveStackTrace() {
        fprintf(stderr, "[TRACE] %s: Entered (use_backtrace not defined)\n", __func__);
        if (current_exception_message == NULL) {
            fprintf(stderr, "[TRACE] %s: current_exception_message is NULL, returning\n", __func__);
            fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
            return;
        }
        // put the existing message in the stack trace buffer and free it for the new message
        if (current_stacktrace_strings == NULL) {
            fprintf(stderr, "[TRACE] %s: current_stacktrace_strings is NULL\n", __func__);
            current_stacktrace_strings = malloc(sizeof(char*));
            current_stacktrace_strings[0] = current_exception_message;
            current_stacktrace_size = 1;
            fprintf(stderr, "[TRACE] %s: Allocated current_stacktrace_strings %p, size %zu, added message '%s'\n", __func__, (void*)current_stacktrace_strings, current_stacktrace_size, current_exception_message);
        } else {
            fprintf(stderr, "[TRACE] %s: current_stacktrace_strings is not NULL, size %zu\n", __func__, current_stacktrace_size);
            size_t combined_size = current_stacktrace_size + 1;
            char** combined_strings = malloc(combined_size * sizeof(char*));
            fprintf(stderr, "[TRACE] %s: Allocated combined_strings %p, size %zu\n", __func__, (void*)combined_strings, combined_size);
            for (size_t i = 0; i < current_stacktrace_size; i++) {
                combined_strings[i] = current_stacktrace_strings[i];
            }
            combined_strings[current_stacktrace_size] = current_exception_message;
            fprintf(stderr, "[TRACE] %s: Added message '%s' at index %zu\n", __func__, current_exception_message, current_stacktrace_size);
            fprintf(stderr, "[TRACE] %s: Freeing old current_stacktrace_strings %p\n", __func__, (void*)current_stacktrace_strings);
            free(current_stacktrace_strings);
            current_stacktrace_strings = combined_strings;
            current_stacktrace_size = combined_size;
            fprintf(stderr, "[TRACE] %s: Updated current_stacktrace_strings to %p and size to %zu\n", __func__, (void*)current_stacktrace_strings, current_stacktrace_size);
        }
        current_exception_message = NULL; // Message is now stored in stack trace, set current message to NULL for the new exception
        fprintf(stderr, "[TRACE] %s: Reset current_exception_message to NULL\n", __func__);
        fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
    }
    void printStackTrace() {
        fprintf(stderr, "[TRACE] %s: Entered (use_backtrace not defined)\n", __func__);
        fprintf(stderr, "[TRACE] %s: current_stacktrace_size = %zu\n", __func__, current_stacktrace_size);
        for (size_t i = 1; i < current_stacktrace_size; i++) { // skip the first message, which is the current exception message
            fprintf(stderr, "[TRACE] %s: Printing line %zu: '%s'\n", __func__, i, current_stacktrace_strings[i]);
            fprintf(stderr, "\tWhile handling exception: %s\n", current_stacktrace_strings[i]);
            fprintf(stderr, "[TRACE] %s: Freeing stack trace string %p\n", __func__, (void*)current_stacktrace_strings[i]);
            free(current_stacktrace_strings[i]); // Free messages stored in the stack trace buffer
        }
        if (current_stacktrace_strings != NULL) {
             fprintf(stderr, "[TRACE] %s: Freeing current_stacktrace_strings %p\n", __func__, (void*)current_stacktrace_strings);
            free(current_stacktrace_strings);
            current_stacktrace_strings = NULL;
            current_stacktrace_size = 0;
             fprintf(stderr, "[TRACE] %s: Reset current_stacktrace_strings to NULL and size to 0\n", __func__);
        }
        fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
    }

#endif


void raiseException(int status, char* formatstr, ...) {
    fprintf(stderr, "[TRACE] %s: Entered with status %d, formatstr '%s'\n", __func__, status, formatstr);
    saveStackTrace(); // call it first so that the stack trace is saved with the old message before the message is overwritten
    fprintf(stderr, "[TRACE] %s: saveStackTrace called\n", __func__);

    if (current_exception_message != NULL) {
        fprintf(stderr, "[TRACE] %s: current_exception_message is not NULL, freeing %p\n", __func__, (void*)current_exception_message);
        free(current_exception_message);
        current_exception_message = NULL;
    }
    if (formatstr != NULL) {
        fprintf(stderr, "[TRACE] %s: formatstr is not NULL\n", __func__);
        size_t formatstr_len = strlen(formatstr);
        if (formatstr[formatstr_len - 1] != '\n') {
            fprintf(stderr, "[TRACE] %s: formatstr does not end with newline, adding one\n", __func__);
            char* new_formatstr = malloc(formatstr_len + 2);
            strcpy(new_formatstr, formatstr);
            new_formatstr[formatstr_len] = '\n';
            new_formatstr[formatstr_len + 1] = '\0';
            // free(formatstr); can't free formatstr because it's a pointer to a string literal
            formatstr = new_formatstr;
            fprintf(stderr, "[TRACE] %s: New formatstr: '%s' at %p\n", __func__, formatstr, (void*)formatstr);
        }
        va_list args;
        va_start(args, formatstr);
        int res = vasprintf(&current_exception_message, formatstr, args);
        va_end(args);
        fprintf(stderr, "[TRACE] %s: vasprintf returned %d, current_exception_message set to '%s' at %p\n", __func__, res, current_exception_message, (void*)current_exception_message);
        // If new_formatstr was allocated, free it after vasprintf
        if (formatstr[formatstr_len] == '\n' && formatstr[formatstr_len+1] == '\0') { // check if we added the newline
             fprintf(stderr, "[TRACE] %s: Freeing temporary new_formatstr %p\n", __func__, (void*)formatstr);
            free(formatstr); // free the allocated new_formatstr
        }
    }
    fprintf(stderr, "[TRACE] %s: Performing siglongjmp to %p with status %d\n", __func__, (void*)current_exception_buffer, status);
    siglongjmp(*current_exception_buffer, status);
    // Should not reach here
    fprintf(stderr, "[TRACE] %s: ERROR: Should not reach here after siglongjmp!\n", __func__);
}

void printException() {
    fprintf(stderr, "[TRACE] %s: Entered\n", __func__);
    if (current_exception_message == NULL || strlen(current_exception_message) == 0){
        fprintf(stderr, "[TRACE] %s: current_exception_message is NULL or empty\n", __func__);
        fprintf(stderr, "Error thrown with no message\n");
    } else {
        fprintf(stderr, "[TRACE] %s: Printing current_exception_message '%s'\n", __func__, current_exception_message);
        fprintf(stderr, "%s\n", current_exception_message);
        fprintf(stderr, "[TRACE] %s: Freeing current_exception_message %p\n", __func__, (void*)current_exception_message);
        free(current_exception_message);
        current_exception_message = NULL;
        fprintf(stderr, "[TRACE] %s: Reset current_exception_message to NULL\n", __func__);
    }
    fprintf(stderr, "[TRACE] %s: Calling printStackTrace\n", __func__);
    printStackTrace();
    fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
}

#define SEGFAULT_SECTION_UNSET "(unset)";
_Thread_local const char* SEGFAULT_SECTION = SEGFAULT_SECTION_UNSET;

void setCodeSectionForSegfaultHandler(const char* section) {
    fprintf(stderr, "[TRACE] %s: Entered with section '%s'\n", __func__, section);
    SEGFAULT_SECTION = section;
    fprintf(stderr, "[TRACE] %s: SEGFAULT_SECTION set to '%s'\n", __func__, SEGFAULT_SECTION);
    fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
}
void unsetCodeSectionForSegfaultHandler() {
    fprintf(stderr, "[TRACE] %s: Entered\n", __func__);
    SEGFAULT_SECTION = SEGFAULT_SECTION_UNSET;
    fprintf(stderr, "[TRACE] %s: SEGFAULT_SECTION set to '%s'\n", __func__, SEGFAULT_SECTION);
    fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
}



pthread_t main_thread_id;
void set_main_threadid() {
    fprintf(stderr, "[TRACE] %s: Entered\n", __func__);
    main_thread_id = pthread_self();
    fprintf(stderr, "[TRACE] %s: main_thread_id set to %lu\n", __func__, (unsigned long)main_thread_id);
    fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
}

bool is_main_thread() {
    fprintf(stderr, "[TRACE] %s: Entered\n", __func__);
    bool result = pthread_equal(main_thread_id, pthread_self());
    fprintf(stderr, "[TRACE] %s: main_thread_id is %lu, current thread_id is %lu, result is %s\n", __func__, (unsigned long)main_thread_id, (unsigned long)pthread_self(), result ? "true" : "false");
    fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
    return result;
}

void terminateThread() {
    fprintf(stderr, "[TRACE] %s: Entered. Terminating thread %lu\n", __func__, (unsigned long)pthread_self());
    pthread_exit(NULL);
    // Should not reach here
    fprintf(stderr, "[TRACE] %s: ERROR: Should not reach here after pthread_exit!\n", __func__);
}

#ifndef _WIN32
void* get_instruction_pointer(ucontext_t *context) {
    fprintf(stderr, "[TRACE] %s: Entered with context %p\n", __func__, (void*)context);

    uintptr_t ip = 0;
    void* result_ip = NULL;

    #if defined(__APPLE__)
        #if defined(__x86_64__)
            ip = (uintptr_t)context->uc_mcontext->__ss.__rip;
            fprintf(stderr, "[TRACE] %s: __APPLE__ __x86_64__, got ip %p\n", __func__, (void*)ip);
        #elif defined(__i386__)
            ip = (uintptr_t)context->uc_mcontext->__ss.__eip;
            fprintf(stderr, "[TRACE] %s: __APPLE__ __i386__, got ip %p\n", __func__, (void*)ip);
        #elif defined(__aarch64__)
            ip = (uintptr_t)context->uc_mcontext->__ss.__pc;
            fprintf(stderr, "[TRACE] %s: __APPLE__ __aarch64__, got ip %p\n", __func__, (void*)ip);
        #elif defined(__arm__)
            ip = (uintptr_t)context->uc_mcontext->__ss.__pc;
            fprintf(stderr, "[TRACE] %s: __APPLE__ __arm__, got ip %p\n", __func__, (void*)ip);
        #elif defined(__ppc__)
            ip = (uintptr_t)context->uc_mcontext->__ss.__srr0;
            fprintf(stderr, "[TRACE] %s: __APPLE__ __ppc__, got ip %p\n", __func__, (void*)ip);
        #else
            fprintf(stderr, "[TRACE] %s: __APPLE__ unknown arch, returning NULL\n", __func__);
            fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
            return NULL;
        #endif
    #elif defined(__linux__)
        #if defined(__x86_64__)
        fprintf(stderr, "[TRACE] %s: __linux__ __x86_64__, returning NULL (not implemented)\n", __func__);
        fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
        return NULL;
        #elif defined(__i386__)
        fprintf(stderr, "[TRACE] %s: __linux__ __i386__, returning NULL (not implemented)\n", __func__);
        fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
        return NULL;
        #elif defined(__aarch64__)
            ip = (uintptr_t)context->uc_mcontext.pc;
            fprintf(stderr, "[TRACE] %s: __linux__ __aarch64__, got ip %p\n", __func__, (void*)ip);
        #elif defined(__arm__)
            ip = (uintptr_t)context->uc_mcontext.arm_pc;
            fprintf(stderr, "[TRACE] %s: __linux__ __arm__, got ip %p\n", __func__, (void*)ip);
        #elif defined(__ppc__)
            ip = (uintptr_t)context->uc_mcontext.regs->nip;
            fprintf(stderr, "[TRACE] %s: __linux__ __ppc__, got ip %p\n", __func__, (void*)ip);
        #elif defined(__mips__)
            ip = (uintptr_t)context->uc_mcontext.pc;
            fprintf(stderr, "[TRACE] %s: __linux__ __mips__, got ip %p\n", __func__, (void*)ip);
        #elif defined(__sparc__)
        fprintf(stderr, "[TRACE] %s: __linux__ __sparc__, returning NULL (not implemented)\n", __func__);
        fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
        return NULL;
        #elif defined(__riscv)
        fprintf(stderr, "[TRACE] %s: __linux__ __riscv__, returning NULL (not implemented)\n", __func__);
        fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
        return NULL;
        #elif defined(__alpha__)
            ip = (uintptr_t)context->uc_mcontext.sc_pc;
            fprintf(stderr, "[TRACE] %s: __linux__ __alpha__, got ip %p\n", __func__, (void*)ip);
        #elif defined(__ia64__)
            ip = (uintptr_t)context->uc_mcontext.sc_ip;
            fprintf(stderr, "[TRACE] %s: __linux__ __ia64__, got ip %p\n", __func__, (void*)ip);
        #elif defined(__s390__)
            ip = (uintptr_t)context->uc_mcontext.psw.addr;
            fprintf(stderr, "[TRACE] %s: __linux__ __s390__, got ip %p\n", __func__, (void*)ip);
        #elif defined(__powerpc__)
        fprintf(stderr, "[TRACE] %s: __linux__ __powerpc__, returning NULL (not implemented)\n", __func__);
        fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
        return NULL;
        #else
            fprintf(stderr, "[TRACE] %s: __linux__ unknown arch, returning NULL\n", __func__);
            fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
            return NULL;
        #endif
    #elif defined(__FreeBSD__)
        #if defined(__x86_64__)
            ip = (uintptr_t)context->uc_mcontext.mc_rip;
            fprintf(stderr, "[TRACE] %s: __FreeBSD__ __x86_64__, got ip %p\n", __func__, (void*)ip);
        #elif defined(__i386__)
            ip = (uintptr_t)context->uc_mcontext.mc_eip;
            fprintf(stderr, "[TRACE] %s: __FreeBSD__ __i386__, got ip %p\n", __func__, (void*)ip);
        #elif defined(__aarch64__)
            ip = (uintptr_t)context->uc_mcontext.mc_gpregs.gp_elr;
            fprintf(stderr, "[TRACE] %s: __FreeBSD__ __aarch64__, got ip %p\n", __func__, (void*)ip);
        #elif defined(__arm__)
            ip = (uintptr_t)context->uc_mcontext.mc_gpregs.gp_pc;
            fprintf(stderr, "[TRACE] %s: __FreeBSD__ __arm__, got ip %p\n", __func__, (void*)ip);
        #elif defined(__ppc__)
            ip = (uintptr_t)context->uc_mcontext.mc_srr0;
            fprintf(stderr, "[TRACE] %s: __FreeBSD__ __ppc__, got ip %p\n", __func__, (void*)ip);
        #elif defined(__riscv)
        fprintf(stderr, "[TRACE] %s: __FreeBSD__ __riscv__, returning NULL (not implemented)\n", __func__);
        fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
        return NULL;
        #elif defined(__alpha__)
            ip = (uintptr_t)context->uc_mcontext.mc_pc;
            fprintf(stderr, "[TRACE] %s: __FreeBSD__ __alpha__, got ip %p\n", __func__, (void*)ip);
        #elif defined(__ia64__)
            ip = (uintptr_t)context->uc_mcontext.mc_ip;
            fprintf(stderr, "[TRACE] %s: __FreeBSD__ __ia64__, got ip %p\n", __func__, (void*)ip);
        #else
            fprintf(stderr, "[TRACE] %s: __FreeBSD__ unknown arch, returning NULL\n", __func__);
            fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
            return NULL;
        #endif
    #else
        fprintf(stderr, "[TRACE] %s: Unknown OS, returning NULL\n", __func__);
        fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
        return NULL;
    #endif

    result_ip = (void*)ip;
    fprintf(stderr, "[TRACE] %s: Returning instruction pointer %p\n", __func__, result_ip);
    fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
    return result_ip;
}

void segfault_handler(int sig, siginfo_t *info, void *ucontext) {
    fprintf(stderr, "[TRACE] %s: Entered with signal %d, info %p, ucontext %p\n", __func__, sig, (void*)info, (void*)ucontext);
    // Cast ucontext to ucontext_t to get register info
    ucontext_t *context = (ucontext_t *)ucontext;
    fprintf(stderr, "[TRACE] %s: Cast ucontext to context %p\n", __func__, (void*)context);

    // Get the address where the fault occurred
    void *fault_address = info->si_addr;
    fprintf(stderr, "[TRACE] %s: Fault address: %p\n", __func__, fault_address);

    char* segfault_message;
    void *ip = get_instruction_pointer(context);
    fprintf(stderr, "[TRACE] %s: Instruction pointer: %p\n", __func__, ip);
    // Print fault address
    asprintf(&segfault_message,
     "Segmentation fault at address: %p\n"
     "Instruction pointer: %p\n"
     "In Section: %s\n"
     , fault_address, ip, SEGFAULT_SECTION);
     fprintf(stderr, "[TRACE] %s: Created segfault_message '%s' at %p\n", __func__, segfault_message, (void*)segfault_message);

     current_exception_message = segfault_message;
     fprintf(stderr, "[TRACE] %s: current_exception_message set to %p\n", __func__, (void*)current_exception_message);

    // Get the stack trace

    if(!is_main_thread()){
        fprintf(stderr, "[TRACE] %s: Caught segfault on non-main thread\n", __func__);
        fprintf(stderr, "Caught segfault on non-main thread. Terminating thread.\n");
        printf("%s\n", segfault_message); // Print to stdout? Original code does this. Keeping it.
        saveStackTrace();
        fprintf(stderr, "[TRACE] %s: saveStackTrace called\n", __func__);
        printStackTrace();
        fprintf(stderr, "[TRACE] %s: printStackTrace called\n", __func__);
        if (isTestEnvExit1OnFail) {
            fprintf(stderr, "[TRACE] %s: isTestEnvExit1OnFail is true, exiting with status 1\n", __func__);
            exit(1);
        }
        terminateThread(); // This calls pthread_exit, should not return
        fprintf(stderr, "[TRACE] %s: ERROR: Should not reach here after terminateThread!\n", __func__);
    } else {
        fprintf(stderr, "[TRACE] %s: Caught segfault on main thread\n", __func__);
        raiseException(1, "%s\n", segfault_message); // This calls siglongjmp, should not return
         fprintf(stderr, "[TRACE] %s: ERROR: Should not reach here after raiseException!\n", __func__);
    }
    fprintf(stderr, "[TRACE] %s: Exited\n", __func__); // Should not be reached
}
#else
void segfault_handler(int signal) {
    fprintf(stderr, "[TRACE] %s: Entered with signal %d (Windows)\n", __func__, signal);

    if (!is_main_thread()) {
        fprintf(stderr, "[TRACE] %s: Caught segfault on non-main thread (Windows)\n", __func__);
        fprintf(stderr, "Caught segfault on non-main thread. Terminating thread.\n");
        terminateThread(); // Should not reach here
        fprintf(stderr, "[TRACE] %s: ERROR: Should not reach here after terminateThread! (Windows)\n", __func__);
    }
    fprintf(stderr, "[TRACE] %s: Allocating memory for current_exception_message (Windows)\n", __func__);
    current_exception_message = malloc(strlen("Segmentation fault in section: ") + strlen(SEGFAULT_SECTION) + 1);
    fprintf(stderr, "[TRACE] %s: Allocated %p (Windows)\n", __func__, (void*)current_exception_message);
    strcpy(current_exception_message, "Segmentation fault in section: ");
    strcat(current_exception_message, SEGFAULT_SECTION);
     fprintf(stderr, "[TRACE] %s: Set current_exception_message to '%s' (Windows)\n", __func__, current_exception_message);


    if (!is_main_thread()) { // This check is repeated, keeping it as per original code
        fprintf(stderr, "[TRACE] %s: (Repeat check) Caught segfault on non-main thread (Windows)\n", __func__);
        fprintf(stderr, "Caught segfault on non-main thread. Terminating thread.\n");
        printStackTrace();
        fprintf(stderr, "[TRACE] %s: printStackTrace called (Windows)\n", __func__);
        if (isTestEnvExit1OnFail) {
             fprintf(stderr, "[TRACE] %s: isTestEnvExit1OnFail is true, exiting with status 1 (Windows)\n", __func__);
            exit(1);
        }
        terminateThread(); // Should not reach here
         fprintf(stderr, "[TRACE] %s: ERROR: Should not reach here after terminateThread! (Windows)\n", __func__);
    }

    raiseException(1, "Segmentation fault in section: %s\n", SEGFAULT_SECTION); // This calls siglongjmp, should not return
    // siglongjmp(*current_exception_buffer, 1); // Original comment, raiseException does this internally

    fprintf(stderr, "[TRACE] %s: Exited (Windows)\n", __func__); // Should not be reached
}

#endif


void install_root_exception_handler() {
    fprintf(stderr, "[TRACE] %s: Entered\n", __func__);
    set_main_threadid();
    fprintf(stderr, "[TRACE] %s: set_main_threadid called\n", __func__);
    int jmp_status = sigsetjmp(rootJmpBuffer, 1);
    fprintf(stderr, "[TRACE] %s: sigsetjmp returned %d\n", __func__, jmp_status);
    if (jmp_status != 0) {
        fprintf(stderr, "[TRACE] %s: sigsetjmp returned non-zero (%d), exception caught\n", __func__, jmp_status);
        if (is_main_thread()){
            fprintf(stderr, "[TRACE] %s: Caught exception on main thread\n", __func__);
            fprintf(stderr,"Root exception handler caught an exception on main thread\n");
            printException();
            fprintf(stderr, "[TRACE] %s: printException called, exiting with status 1\n", __func__);
            exit(1);
        } else {
            fprintf(stderr, "[TRACE] %s: Caught exception on non-main thread\n", __func__);
            fprintf(stderr,"Root exception handler caught an exception on non-main thread. Terminating thread.\n");
            printException();
            fprintf(stderr, "[TRACE] %s: printException called\n", __func__);
            if (isTestEnvExit1OnFail) {
                fprintf(stderr, "[TRACE] %s: isTestEnvExit1OnFail is true, exiting with status 1\n", __func__);
                exit(1);
            }
            terminateThread(); // Should not reach here
            fprintf(stderr, "[TRACE] %s: ERROR: Should not reach here after terminateThread!\n", __func__);
        }
    }
    fprintf(stderr, "[TRACE] %s: sigsetjmp returned 0, handler installed. Exited.\n", __func__);
}
void install_segfault_handler() {
    fprintf(stderr, "[TRACE] %s: Entered\n", __func__);
    #ifndef _WIN32
    fprintf(stderr, "[TRACE] %s: Installing segfault handler (Non-Windows)\n", __func__);
    struct sigaction sa;
    sa.sa_sigaction = segfault_handler;
    sa.sa_flags = SA_SIGINFO;
    fprintf(stderr, "[TRACE] %s: sigaction struct configured\n", __func__);

    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction SIGSEGV");
        fprintf(stderr, "[TRACE] %s: Error installing SIGSEGV handler, exiting\n", __func__);
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "[TRACE] %s: SIGSEGV handler installed\n", __func__);
    //catch bus errors etc
    sigaction(SIGBUS, &sa, NULL);
    fprintf(stderr, "[TRACE] %s: SIGBUS handler installed\n", __func__);
    sigaction(SIGILL, &sa, NULL);
    fprintf(stderr, "[TRACE] %s: SIGILL handler installed\n", __func__);
    sigaction(SIGFPE, &sa, NULL);
    fprintf(stderr, "[TRACE] %s: SIGFPE handler installed\n", __func__);
    // signal(SIGABRT, segfault_handler); // Original comment, not using sigaction for SIGABRT

    #else
    fprintf(stderr, "[TRACE] %s: Installing segfault handler (Windows)\n", __func__);
    signal(SIGSEGV, segfault_handler);
    fprintf(stderr, "[TRACE] %s: SIGSEGV handler installed\n", __func__);
    // signal(SIGBUS, segfault_handler); <-- doesnt exist on windows
    signal(SIGILL, segfault_handler);
    fprintf(stderr, "[TRACE] %s: SIGILL handler installed\n", __func__);
    signal(SIGFPE, segfault_handler);
    fprintf(stderr, "[TRACE] %s: SIGFPE handler installed\n", __func__);
    // signal(SIGABRT, segfault_handler); // Original comment
    #endif
    fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
}

void main_method_install_exception_handlers() {
    fprintf(stderr, "[TRACE] %s: Entered\n", __func__);
    install_root_exception_handler();
    fprintf(stderr, "[TRACE] %s: install_root_exception_handler called\n", __func__);
    install_segfault_handler();
    fprintf(stderr, "[TRACE] %s: install_segfault_handler called\n", __func__);
    fprintf(stderr, "[TRACE] %s: Exited\n", __func__);
}