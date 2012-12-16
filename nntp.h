#define NNTP_SERVER_READY_POSTING_ALLOWED 200
#define NNTP_SERVER_READY_POSTING_PROHIBITED 201
#define NNTP_MORE_AUTHENTICATION_REQUIRED 381
#define NNTP_AUTHENTICATION_SUCCESSFUL 281
#define NNTP_CONNECTION_CLOSING 205
#define NNTP_SERVICE_TEMPORARILY_UNAVAILABLE 400
#define NNTP_SERVICE_PERMANENTLY_UNAVAILABLE 502

#define CONNECT_TRY_AGAIN_LATER -1
#define CONNECT_FAILURE -2

#define STRING_BUFSIZE 1024

int nntp_get_reply( int sockfd, int code );

int nntp_sendall( int sockfd, const char *buf, size_t len );

int nntp_send_command( int sockfd, int code, const char *command, ... );

int nntp_connect( int *sockfd, const char *hostname, int port, const char *username, const char *password );

int nntp_logoff( int *sockfd );
