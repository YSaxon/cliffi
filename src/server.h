#define DEFAULT_SERVER_PORT "16732" // Example port, choose your own
#define DEFAULT_HOST "0.0.0.0"    // Listen on all interfaces
#define SERVER_BUFFER_SIZE 4096

int start_cliffi_tcp_server(const char* host, const char* port_str, bool fork_per_client);
