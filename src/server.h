#ifndef CLIFFI_SERVER_H
#define CLIFFI_SERVER_H

#define DEFAULT_SERVER_PORT "16732" // Example port, choose your own
#define DEFAULT_SERVER_HOST "127.0.0.1"    // Listen on all interfaces
#define SERVER_BUFFER_SIZE 4096
/**
 * @brief Starts the cliffi TCP server.
 *
 * This function initializes the networking library, binds to the specified host and port,
 * and enters a loop to accept incoming client connections. For each connection,
 * a new thread is spawned to handle the session.
 *
 * @param host The host address to listen on (e.g., "0.0.0.0" for all interfaces).
 * @param port_str The port number as a string (e.g., "16732").
 * @return Returns 0 on clean shutdown (though it loops forever), or 1 on a fatal startup error.
 */
int start_cliffi_tcp_server(const char* host, const char* port_str);

#endif // CLIFFI_SERVER_H
