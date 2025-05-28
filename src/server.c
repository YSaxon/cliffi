#include "repl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Platform-specific includes and definitions
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VAL INVALID_SOCKET
    #define close_socket(s) closesocket(s)
    #define socket_errno WSAGetLastError()
#else // POSIX
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #include <errno.h>
    #include <sys/wait.h> // For waitpid (in forking mode)
    #include <signal.h>   // For signal (in forking mode for SIGCHLD)
    typedef int socket_t;
    #define INVALID_SOCKET_VAL -1
    #define close_socket(s) close(s)
    #define socket_errno errno
#endif

// --- Forward declarations for cliffi internal functions (you'll have these) ---
// Example: This is the function from your cliffi's main.c that processes REPL commands
// You will need to adapt it or create a variant if its current form prints directly
// to stdout and you need to capture that output to send to the socket.
// int parseREPLCommand(char* command); // Original
// For the server, you might need something like:
// char* execute_cliffi_command_and_capture_output(const char* command_string);
// For now, we'll just conceptualize calling your existing function and note where I/O capture is needed.

// --- Cliffi's main exception handling (from your provided code) ---
// #include "exception_handling.h" // Assuming you have this
// void main_method_install_exception_handlers() { /* Your impl */ }
// void printException() { /* Your impl */ }
// #define TRY if(1){
// #define CATCHALL } else {
// #define END_TRY }

char* printf_socket_error_prefix;

// --- Constants ---
#define DEFAULT_SERVER_PORT "16732" // Example port, choose your own
#define DEFAULT_HOST "0.0.0.0"    // Listen on all interfaces
#define SERVER_BUFFER_SIZE 4096

#ifndef _WIN32
// Simple SIGCHLD handler to prevent zombie processes in forking mode
void sigchld_handler(int s) {
    (void)s; // Unused parameter
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}
#endif

/**
 * Handles a single client session.
 * Reads commands, executes them via cliffi, captures output, and sends response.
 */
void handle_client_session(socket_t client_socket) {
    char buffer[SERVER_BUFFER_SIZE];
    int nbytes;
    printf_socket_error_prefix = "[Client PID: %d] "; // Informative prefix if forking
#ifndef _WIN32
    // If forking, each child will have its own PID
    if (getpid() != 0) { // Simplified; in practice, you'd know if you're a child
         printf_socket_error_prefix = "[Client PID: %d] ";
    } else {
         printf_socket_error_prefix = "[Client] "; // Fallback for non-forking or parent
    }
#else
    // Windows does not fork like this, so a generic prefix
    printf_socket_error_prefix = "[Client] ";
#endif

    printf("%sConnected.\n", printf_socket_error_prefix);

    // Welcome message (optional)
    const char* welcome_msg = "Welcome to cliffi server!\n> ";
    if (send(client_socket, welcome_msg, strlen(welcome_msg), 0) == -1) {
        perror("send welcome"); // Use a platform-agnostic error print
        return;
    }

    while ((nbytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[nbytes] = '\0';

        // Trim newline characters often sent by netcat/telnet
        char* p = buffer;
        while (*p) {
            if (*p == '\n' || *p == '\r') {
                *p = '\0';
                break;
            }
            p++;
        }

        if (strlen(buffer) == 0) { // Handle empty lines after trim
            if (send(client_socket, "> ", 2, 0) == -1) break; // Send prompt again
            continue;
        }

        printf("%sReceived command: '%s'\n", printf_socket_error_prefix, buffer);

        if (strcmp(buffer, "exit") == 0 || strcmp(buffer, "quit") == 0) {
            printf("%sClient requested exit.\n", printf_socket_error_prefix);
            break;
        }

        parseREPLCommand(buffer);

        // --- CRITICAL SECTION: Execute command and capture output ---
        // This is where you need to integrate with cliffi's command processing
        // and capture its stdout/stderr.

        // ** OPTION 1: If parseREPLCommand can be made to write to a buffer/pipe **
        //    (This is the cleaner, but potentially more invasive change to cliffi)
        //    char output_buffer[SOME_LARGE_SIZE];
        //    int result = parseREPLCommand_to_buffer(buffer, output_buffer, sizeof(output_buffer));
        //    if (result != 0) { /* handle cliffi error */ }
        //    send(client_socket, output_buffer, strlen(output_buffer), 0);

        // ** OPTION 2: Temporarily redirect stdout/stderr to a pipe or temp file **
        //    (Less invasive to parseREPLCommand, but more OS-level plumbing here)
        //    This is complex to do robustly and cross-platform within this function.
        //    Example pseudo-code for POSIX pipe redirection:
        //    int pipefd[2]; pipe(pipefd);
        //    int saved_stdout = dup(STDOUT_FILENO);
        //    dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe write end
        //    close(pipefd[1]); // Close write end in this process after dup
        //
        //    TRY {
        //        parseREPLCommand(buffer); // This now writes to the pipe
        //    } CATCHALL {
        //        printException_to_pipe_or_buffer(); // Need to capture this too
        //    } END_TRY
        //
        //    fflush(stdout); // Ensure all data is written to pipe
        //    dup2(saved_stdout, STDOUT_FILENO); // Restore stdout
        //    close(saved_stdout);
        //
        //    char captured_output[SERVER_BUFFER_SIZE];
        //    int output_len = read(pipefd[0], captured_output, sizeof(captured_output)-1);
        //    close(pipefd[0]);
        //    if (output_len > 0) {
        //        captured_output[output_len] = '\0';
        //        send(client_socket, captured_output, output_len, 0);
        //    } else {
        //        send(client_socket, "No output or error reading output.\n", 33, 0);
        //    }
        //
        // For now, a placeholder response:
        char response[SERVER_BUFFER_SIZE];
        snprintf(response, sizeof(response), "Executing '%s' (output capture TBD)\n", buffer);
        // This is where you would call your actual cliffi command processor
        // e.g. by calling a modified parseREPLCommand or execute_cliffi_command_and_capture_output
        // For now, just echoing back for testing the server shell.
        // In a real scenario, you'd call `parseREPLCommand(buffer)` after setting up I/O redirection.
        // Then you'd read the captured output and send it.

        // Simulate calling parseREPLCommand and it printing something.
        // This part needs actual implementation of output capture.
        // For this skeleton, let's just send back an ack.
        if (send(client_socket, response, strlen(response), 0) == -1) {
            // perror_win_or_posix("send response"); // Use a platform-agnostic error print
            break;
        }
        // --- END CRITICAL SECTION ---

        // Send prompt for next command
        if (send(client_socket, "> ", 2, 0) == -1) {
            // perror_win_or_posix("send prompt");
            break;
        }
    }

    if (nbytes == 0) {
        printf("%sClient disconnected gracefully.\n", printf_socket_error_prefix);
    } else if (nbytes < 0) {
        // perror_win_or_posix("recv");
        fprintf(stderr, "%sRecv error: %d\n", printf_socket_error_prefix, socket_errno);
    }

    printf("%sSession ended.\n", printf_socket_error_prefix);
    close_socket(client_socket);
}


/**
 * Starts the cliffi TCP server.
 */
int start_cliffi_tcp_server(const char* host, const char* port_str, bool fork_per_client) {
    socket_t listen_fd = INVALID_SOCKET_VAL;
    socket_t client_fd = INVALID_SOCKET_VAL;
    int status;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed. Error Code: %d\n", socket_errno);
        return 1;
    }
    if (fork_per_client) {
        fprintf(stderr, "Warning: Forking mode is not supported on Windows. Using single client mode.\n");
        fork_per_client = false;
    }
#else // POSIX
    if (fork_per_client) {
        struct sigaction sa;
        sa.sa_handler = sigchld_handler; // Reap all dead processes
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
        if (sigaction(SIGCHLD, &sa, 0) == -1) {
            perror("sigaction");
            return 1;
        }
        printf("Server starting in fork-per-client mode.\n");
    } else {
        printf("Server starting in single-client-at-a-time mode.\n");
    }
#endif

    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((status = getaddrinfo(host, port_str, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    // Loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((listen_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == INVALID_SOCKET_VAL) {
            perror("server: socket");
            continue;
        }

        int yes = 1;
        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes)) == -1) {
            perror("setsockopt SO_REUSEADDR");
            close_socket(listen_fd);
            freeaddrinfo(servinfo);
            return 1;
        }

        if (bind(listen_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close_socket(listen_fd);
            perror("server: bind");
            continue;
        }
        break; // If we got here, we successfully bound
    }

    freeaddrinfo(servinfo); // All done with this structure

    if (p == NULL) {
        fprintf(stderr, "Server: failed to bind to host %s, port %s\n", host, port_str);
        return 1;
    }

    if (listen(listen_fd, 10) == -1) { // 10 is a common backlog value
        perror("listen");
        return 1;
    }

    printf("Server listening on %s:%s...\n", host, port_str);

    // Main accept() loop
    while (1) {
        struct sockaddr_storage client_addr;
        socklen_t sin_size = sizeof client_addr;
        client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &sin_size);

        if (client_fd == INVALID_SOCKET_VAL) {
            perror("accept");
            continue; // Or handle error more gracefully
        }

        // Get client IP for logging (optional)
        char s[INET6_ADDRSTRLEN];
        inet_ntop(client_addr.ss_family,
                  &(((struct sockaddr_in*)&client_addr)->sin_addr), // Basic IPv4 assumption here for simplicity
                  s, sizeof s);
        printf("Server: got connection from %s\n", s);


        if (fork_per_client) {
#ifndef _WIN32 // Forking only on POSIX
            pid_t pid = fork();
            if (pid == -1) {
                perror("fork");
                close_socket(client_fd); // Close client socket in parent if fork failed
            } else if (pid == 0) { // Child process
                close_socket(listen_fd); // Child doesn't need the listener
                handle_client_session(client_fd);
                printf("[Child PID %d] Terminating.\n", getpid());
                exit(0); // Child exits after handling session
            } else { // Parent process
                printf("Server: dispatched client to child PID %d\n", pid);
                close_socket(client_fd); // Parent closes its copy of client socket
            }
#else
            // This path should ideally not be reached if fork_per_client is true on Windows
            // as it's set to false at the beginning of this function.
            // Fallback to single client handling if somehow fork_per_client is true on Windows.
            handle_client_session(client_fd);
#endif
        } else { // Single client mode
            printf("Server: handling client in single mode.\n");
            handle_client_session(client_fd);
            printf("Server: client session ended, waiting for new connection.\n");
        }
    }

    // Cleanup (though the loop above is infinite, this is good practice if it could break)
    close_socket(listen_fd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}