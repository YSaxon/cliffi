
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
#if (defined(__i386__) || defined(__x86_64__) || defined(__riscv)) && !defined(_WIN32) && !defined(__APPLE__)
    #include <ucontext.h>
#endif
#include "shims.h"
#include "exception_handling.h"


bool isTestEnvExit1OnFail = false;

sigjmp_buf rootJmpBuffer;

sigjmp_buf* current_exception_buffer = &rootJmpBuffer;
sigjmp_buf* old_exception_buffer;
char* current_exception_message = NULL;
char** current_stacktrace_strings = NULL;
size_t current_stacktrace_size = 0;

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
            fprintf(stdout, "While handling exception, current_stacktrace exists but current_exception_message does not. We may have called saveStackTrace twice by mistake\n");
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
    // free(current_stacktrace_strings); // removing this to prevent double free
    // current_stacktrace_strings = NULL; // freeing these will only happen in END_TRY
    // current_stacktrace_size = 0;
}
#else
    //just save the previous error message(s) in a buffer in order to print "while handling exception: " in the catch block
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
        for (size_t i = 1; i < current_stacktrace_size; i++) { // skip the first message, which is the current exception message
            fprintf(stderr, "\tWhile handling exception: %s\n", current_stacktrace_strings[i]);
        }
    }

#endif

void clear_stack_trace() {
    if (current_stacktrace_strings == NULL) {
        return;
    }

#ifndef use_backtrace
    // For non-backtrace, we must free each archived message string individually.
    for (size_t i = 0; i < current_stacktrace_size; i++) {
        free(current_stacktrace_strings[i]);
    }
#endif

    // In both cases, we free the array of pointers itself.
    // For use_backtrace, backtrace_symbols allocates the strings and the array
    // in one block, so a single free is correct.
    free(current_stacktrace_strings);
    current_stacktrace_strings = NULL;
    current_stacktrace_size = 0;
}


void raiseException(int status, char* formatstr, ...) {
    saveStackTrace(); // call it first so that the stack trace is saved with the old message before the message is overwritten

    if (current_exception_message != NULL) {
        free(current_exception_message);
        current_exception_message = NULL;
    }
    if (formatstr != NULL) {
        bool should_free_formatstr = false;
        size_t formatstr_len = strlen(formatstr);
        if (formatstr[formatstr_len - 1] != '\n') {
            char* new_formatstr = malloc(formatstr_len + 2);
            strcpy(new_formatstr, formatstr);
            new_formatstr[formatstr_len] = '\n';
            new_formatstr[formatstr_len + 1] = '\0';
            formatstr = new_formatstr;
            should_free_formatstr = true;
        }
        va_list args;
        va_start(args, formatstr);
        vasprintf(&current_exception_message, formatstr, args);
        va_end(args);
        if (should_free_formatstr) {
            free(formatstr);
        }
    }
    siglongjmp(*current_exception_buffer, status);
}

void printException() {
    if (current_exception_message == NULL || strlen(current_exception_message) == 0){
        fprintf(stderr, "Error thrown with no message\n");
    } else {
        fprintf(stderr, "%s\n", current_exception_message);
        // free(current_exception_message); // removing this from here to prevent double free
        // current_exception_message = NULL; // freeing these will only happen in END_TRY
    }
    printStackTrace();
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

#ifndef _WIN32
void* get_instruction_pointer(ucontext_t *context) {

    uintptr_t ip = 0;

    #if defined(__APPLE__)
        #if defined(__x86_64__)
            ip = (uintptr_t)context->uc_mcontext->__ss.__rip;
        #elif defined(__i386__)
            ip = (uintptr_t)context->uc_mcontext->__ss.__eip;
        #elif defined(__aarch64__)
            ip = (uintptr_t)context->uc_mcontext->__ss.__pc;
        #elif defined(__arm__)
            ip = (uintptr_t)context->uc_mcontext->__ss.__pc;
        #elif defined(__ppc__)
            ip = (uintptr_t)context->uc_mcontext->__ss.__srr0;
        #else
            return NULL;
        #endif
    #elif defined(__linux__)
        #if defined(__x86_64__)
        return NULL;
        #elif defined(__i386__)
        return NULL;
        #elif defined(__aarch64__)
            ip = (uintptr_t)context->uc_mcontext.pc;
        #elif defined(__arm__)
            ip = (uintptr_t)context->uc_mcontext.arm_pc;
        #elif defined(__ppc__)
            ip = (uintptr_t)context->uc_mcontext.regs->nip;
        #elif defined(__mips__)
            ip = (uintptr_t)context->uc_mcontext.pc;
        #elif defined(__sparc__)
        return NULL;
        #elif defined(__riscv)
        return NULL;
        #elif defined(__alpha__)
            ip = (uintptr_t)context->uc_mcontext.sc_pc;
        #elif defined(__ia64__)
            ip = (uintptr_t)context->uc_mcontext.sc_ip;
        #elif defined(__s390__)
            ip = (uintptr_t)context->uc_mcontext.psw.addr;
        #elif defined(__powerpc__)
        return NULL;
        #else
            return NULL;
        #endif
    #elif defined(__FreeBSD__)
        #if defined(__x86_64__)
            ip = (uintptr_t)context->uc_mcontext.mc_rip;
        #elif defined(__i386__)
            ip = (uintptr_t)context->uc_mcontext.mc_eip;
        #elif defined(__aarch64__)
            ip = (uintptr_t)context->uc_mcontext.mc_gpregs.gp_elr;
        #elif defined(__arm__)
            ip = (uintptr_t)context->uc_mcontext.mc_gpregs.gp_pc;
        #elif defined(__ppc__)
            ip = (uintptr_t)context->uc_mcontext.mc_srr0;
        #elif defined(__riscv)
        return NULL;
        #elif defined(__alpha__)
            ip = (uintptr_t)context->uc_mcontext.mc_pc;
        #elif defined(__ia64__)
            ip = (uintptr_t)context->uc_mcontext.mc_ip;
        #else
            return NULL;
        #endif
    #else
        return NULL;
    #endif

    return (void*)ip;
}

void segfault_handler(int sig, siginfo_t *info, void *ucontext) {
    // Cast ucontext to ucontext_t to get register info
    ucontext_t *context = (ucontext_t *)ucontext;

    // Get the address where the fault occurred
    void *fault_address = info->si_addr;

    char* segfault_message;
    void *ip = get_instruction_pointer(context);
    // Print fault address
    asprintf(&segfault_message,
     "\nSegmentation fault at address: %p\n"
     "Instruction pointer: %p\n"
     "In Section: %s"
     , fault_address, ip, SEGFAULT_SECTION);

    raiseException(1, "%s", segfault_message);
}
#else
void segfault_handler(int signal) {
    // if (!is_main_thread()) {
    //     fprintf(stderr, "Caught segfault on non-main thread. Terminating thread.\n");
    //     printStackTrace();
    //     if (isTestEnvExit1OnFail) exit(1);
    //     terminateThread();
    // }
    current_exception_message = malloc(strlen("Segmentation fault in section: ") + strlen(SEGFAULT_SECTION) + 1);
    strcpy(current_exception_message, "Segmentation fault in section: ");
    strcat(current_exception_message, SEGFAULT_SECTION);

    raiseException(1, "Segmentation fault in section: %s\n", SEGFAULT_SECTION);
    // siglongjmp(*current_exception_buffer, 1);
}

#endif


void install_root_exception_handler() {
    set_main_threadid();
    if (sigsetjmp(rootJmpBuffer, 1) != 0) {
        fprintf(stderr, "Uncaught exception. This shouldn't generally ever happen. Please report.\n");
        printException(); // Always print the error details
        exit(1);
    }
}
void install_segfault_handler() {
    #ifndef _WIN32
    struct sigaction sa;
    sa.sa_sigaction = segfault_handler;
    sa.sa_flags = SA_SIGINFO;

    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    //catch bus errors etc
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    // signal(SIGABRT, segfault_handler);

    #else
    signal(SIGSEGV, segfault_handler);
    // signal(SIGBUS, segfault_handler); <-- doesnt exist on windows
    signal(SIGILL, segfault_handler);
    signal(SIGFPE, segfault_handler);
    // signal(SIGABRT, segfault_handler);
    #endif


}

void main_method_install_exception_handlers() {
    install_root_exception_handler();
    install_segfault_handler();
}

