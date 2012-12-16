#define SOCKET_FAILED -1
#define SOCKET_EMPTY  -2
#define SOCKET_TRY_LATER -3
#define SOCKET_UNEXPECTED_RESPONSE -4
#define SOCKET_SEND_INCOMPLETE -5
#define SOCKET_UNKNOWN_HOST -6
#define SOCKET_CANT_POST -7

int socket_open( const char *hostname, int port );