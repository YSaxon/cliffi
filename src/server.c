#include "server.h"
#include "subprocess.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// --- Platform-specific Socket and Threading Includes ---
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <process.h> // For _beginthreadex
    #pragma comment(lib, "ws2_32.lib") // Link with the Winsock library
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VAL INVALID_SOCKET
    #define close_socket(s) closesocket(s)
#else // POSIX (Linux, macOS, etc.)
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <pthread.h>
    #include <netdb.h> // For getaddrinfo
    typedef int socket_t;
    #define INVALID_SOCKET_VAL -1
    #define close_socket(s) close(s)
#endif


// This global must be set to argv[0] in main.c so the server can re-spawn itself.
extern char* g_executable_path;

typedef struct {
    socket_t client_socket;
    struct subprocess_s repl_process;
    bool session_active;
} session_data_t;

typedef struct {
    session_data_t* session;
    volatile bool* is_active_flag;
} proxy_thread_args_t;


#ifdef _WIN32
unsigned __stdcall proxy_socket_to_process(void* args) {
#else
void* proxy_socket_to_process(void* args) {
#endif
    proxy_thread_args_t* proxy_args = (proxy_thread_args_t*)args;
    session_data_t* session = proxy_args->session;
    char buffer[4096];
    int bytes_received;

    FILE* p_stdin = subprocess_stdin(&session->repl_process);

    while (*(proxy_args->is_active_flag)) {
        bytes_received = recv(session->client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            break; // Client disconnected or error
        }
        fwrite(buffer, 1, bytes_received, p_stdin);
        fflush(p_stdin);
    }

    fclose(p_stdin);
    *(proxy_args->is_active_flag) = false;
    free(proxy_args);
    return 0;
}

#ifdef _WIN32
unsigned __stdcall proxy_process_to_socket(void* args) {
#else
void* proxy_process_to_socket(void* args) {
#endif
    proxy_thread_args_t* proxy_args = (proxy_thread_args_t*)args;
    session_data_t* session = proxy_args->session;
    char buffer[4096];
    unsigned int bytes_read;

    while (*(proxy_args->is_active_flag)) {
        bytes_read = subprocess_read_stdout(&session->repl_process, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            send(session->client_socket, buffer, bytes_read, 0);
        }

        bytes_read = subprocess_read_stderr(&session->repl_process, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            send(session->client_socket, buffer, bytes_read, 0);
        }

        if (!subprocess_alive(&session->repl_process)) {
            break;
        }

        #ifdef _WIN32
        Sleep(10);
        #else
        usleep(10000);
        #endif
    }

    *(proxy_args->is_active_flag) = false;
    free(proxy_args);
    return 0;
}

#ifdef _WIN32
unsigned __stdcall handle_client_session_thread(void* session_ptr) {
#else
void* handle_client_session_thread(void* session_ptr) {
#endif
    session_data_t* session = (session_data_t*)session_ptr;
    printf("[Server] New session thread started.\n");

    const char* command_line[] = { g_executable_path, "--repl", NULL };
    int options = subprocess_option_inherit_environment | subprocess_option_enable_async;

    if (subprocess_create(command_line, options, &session->repl_process) != 0) {
        fprintf(stderr, "[Server] Error: Failed to spawn 'cliffi --repl' worker.\n");
        close_socket(session->client_socket);
        free(session);
        return (void*)-1;
    }

    printf("[Server] Worker process spawned.\n");
    session->session_active = true;

    proxy_thread_args_t* args1 = malloc(sizeof(proxy_thread_args_t));
    args1->session = session;
    args1->is_active_flag = &session->session_active;

    proxy_thread_args_t* args2 = malloc(sizeof(proxy_thread_args_t));
    args2->session = session;
    args2->is_active_flag = &session->session_active;

#ifdef _WIN32
    HANDLE hThread1 = (HANDLE)_beginthreadex(NULL, 0, proxy_socket_to_process, args1, 0, NULL);
    HANDLE hThread2 = (HANDLE)_beginthreadex(NULL, 0, proxy_process_to_socket, args2, 0, NULL);
    WaitForSingleObject(hThread1, INFINITE);
    WaitForSingleObject(hThread2, INFINITE);
    CloseHandle(hThread1);
    CloseHandle(hThread2);
#else
    pthread_t tid1, tid2;
    pthread_create(&tid1, NULL, proxy_socket_to_process, args1);
    pthread_create(&tid2, NULL, proxy_process_to_socket, args2);
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
#endif

    printf("[Server] Session ending. Cleaning up.\n");
    close_socket(session->client_socket);
    subprocess_terminate(&session->repl_process);
    subprocess_destroy(&session->repl_process);
    free(session);

    return 0;
}

int start_cliffi_tcp_server(const char* host, const char* port_str) {
    if (g_executable_path == NULL) {
        fprintf(stderr, "[Server] Fatal Error: g_executable_path is not set.\n");
        return 1;
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return 1;
    }
#endif

    socket_t listen_socket = INVALID_SOCKET_VAL;
    struct addrinfo hints, *servinfo, *p;
    int status;
    int yes = 1;

    // Use getaddrinfo for a robust, protocol-agnostic setup.
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // Use my IP

    if ((status = getaddrinfo(host, port_str, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    // Loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        listen_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listen_socket == INVALID_SOCKET_VAL) {
            perror("server: socket");
            continue;
        }

        // Set SO_REUSEADDR to prevent "Address already in use" errors on restart
        if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes)) == -1) {
            perror("setsockopt");
            close_socket(listen_socket);
            freeaddrinfo(servinfo);
            return 1;
        }

        if (bind(listen_socket, p->ai_addr, p->ai_addrlen) == -1) {
            close_socket(listen_socket);
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

    if (listen(listen_socket, 10) < 0) {
        fprintf(stderr, "Failed to listen on socket.\n");
        close_socket(listen_socket);
        return 1;
    }

    printf("[Server] Listening on %s:%s\n", host, port_str);

    while (1) {
        session_data_t* session = (session_data_t*)malloc(sizeof(session_data_t));
        if (!session) {
             fprintf(stderr, "[Server] Fatal: Out of memory.\n"); break;
        }

        session->client_socket = accept(listen_socket, NULL, NULL);

        if (session->client_socket != INVALID_SOCKET_VAL) {
#ifdef _WIN32
            HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, handle_client_session_thread, session, 0, NULL);
            if (hThread) { CloseHandle(hThread); }
#else
            pthread_t tid;
            if (pthread_create(&tid, NULL, handle_client_session_thread, session) == 0) {
                pthread_detach(tid);
            }
#endif
            else {
                fprintf(stderr, "[Server] Failed to create thread for new client.\n");
                close_socket(session->client_socket);
                free(session);
            }
        } else {
            fprintf(stderr, "[Server] Error on accept().\n");
            free(session);
        }
    }

    close_socket(listen_socket);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
