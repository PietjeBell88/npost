#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdarg.h>

#define MIN(a,b) ({ \
    a > b ? b : a; \
})

#define PORT "119"
#define MAXDATASIZE 100
#define STRING_BUFSIZE 1024

#define NNTP_SERVER_READY_POSTING_ALLOWED 200
#define NNTP_SERVER_READY_POSTING_PROHIBITED 201
#define NNTP_MORE_AUTHENTICATION_REQUIRED 381
#define NNTP_AUTHENTICATION_SUCCESSFUL 281
#define NNTP_CONNECTION_CLOSING 205
#define NNTP_SERVICE_TEMPORARILY_UNAVAILABLE 400
#define NNTP_SERVICE_PERMANENTLY_UNAVAILABLE 502

#define SOCKET_FAILED -1
#define SOCKET_EMPTY  -2
#define SOCKET_TRY_LATER -3
#define SOCKET_UNEXPECTED_RESPONSE -4
#define SOCKET_SEND_INCOMPLETE -5
#define SOCKET_UNKNOWN_HOST -6
#define SOCKET_CANT_POST -7

#define CONNECT_TRY_AGAIN_LATER -1
#define CONNECT_FAILURE -2

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int nntp_get_reply( int sockfd, int code )
{
    char buffer[STRING_BUFSIZE];

    int n = recv( sockfd, buffer, 255, 0 );

    // Check for socket errors (i.e. a lack of response)
    if ( n == -1 )
        return SOCKET_FAILED;

    if ( n == 0 )
        return SOCKET_EMPTY;

    // Parse the response
    buffer[n] = '\0';

    printf( "Reply: %s\n", buffer );
    int reply = atoi( buffer );
    printf( "Reply Code: %d\n", reply );

    if ( reply == code )
        return 0;

    if( reply == SOCKET_FAILED || reply == SOCKET_EMPTY )
    {
        printf( "NNTP Reply: No server response (reply=%d)\n", reply );
        return reply;
    }

    else if( reply == NNTP_SERVICE_TEMPORARILY_UNAVAILABLE )
    {
        // Usually a connection limit reached
        printf( "NNTP Reply: Service temporarily unavailable.\n" );
        printf( "Received: \"%s\"\n", buffer );
        return SOCKET_TRY_LATER;
    }

    else if( reply == NNTP_SERVER_READY_POSTING_PROHIBITED ||
             reply == NNTP_SERVICE_PERMANENTLY_UNAVAILABLE )
    {
        // The server info is incorrect
        printf( "NNTP Reply: Not allowed to post to server.\n" );
        printf( "Received: \"%s\"\n", buffer );
        return SOCKET_CANT_POST;
    }

    // Unexpected response. Preferably do not want this to happen.
    printf( "NNTP Reply: Unexpected Reply.\n" );
    printf( "Received: \"%s\"\n", buffer );
    return SOCKET_UNEXPECTED_RESPONSE;
}

int nntp_sendall( int sockfd, const char *buf, size_t len )
{
    size_t total = 0;        // how many bytes we've sent
    size_t bytesleft = len;  // how many we have left to send
    int n;

    while( total < len ) {
        n = send( sockfd, buf+total, MIN(bytesleft,32768), 0 );
        if ( n == -1 )
            break;
        total += n;
        bytesleft -= n;
    }

    if( n == -1 )
        return SOCKET_FAILED;

    if( total == len )
        return 0;

    return SOCKET_SEND_INCOMPLETE;
}

int nntp_send_command( int sockfd, int code, const char *command, ... )
{
    char buffer[STRING_BUFSIZE];
    size_t len;
    int ret;

    va_list args;
    va_start( args, command );
    len = vsprintf( buffer, command, args );
    va_end( args );

    printf( "Sending command: %s\n", buffer );

    ret = nntp_sendall( sockfd, buffer, len );

    if (ret < 0)
        return ret;
    else
        ret = nntp_get_reply( sockfd, code );

    return ret;
}

int socket_open( const char *hostname )
{
    int sockfd; //, numbytes;
    int rv;
    char s[INET6_ADDRSTRLEN];
    char ipstr[INET6_ADDRSTRLEN];

    struct addrinfo hints, *servinfo, *p;

    memset( &hints, 0, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo( hostname, PORT, &hints, &servinfo )) != 0)
    {
        fprintf( stderr, "getaddrinfo: %s\n", gai_strerror(rv) );
        return SOCKET_UNKNOWN_HOST;
    }

    printf("IP addresses for %s:\n\n", hostname);

    for( p = servinfo; p != NULL; p = p->ai_next ) {
        void *addr;
        char *ipver;

        // get the pointer to the address itself,
        // different fields in IPv4 and IPv6:
        if ( p->ai_family == AF_INET ) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }

        // convert the IP to a string and print it:
        inet_ntop( p->ai_family, addr, ipstr, sizeof(ipstr) );
        printf( "  %s: %s\n", ipver, ipstr );
    }

    for( p = servinfo; p != NULL; p = p->ai_next )
    {
        if( (sockfd = socket( p->ai_family, p->ai_socktype, p->ai_protocol )) == -1 )
        {
            perror( "client: socket" );
            continue;
        }

        if (connect( sockfd, p->ai_addr, p->ai_addrlen ) == -1)
        {
            perror( "client: connect" );
            continue;
        }

        break;
    }

    if ( p == NULL )
    {
        fprintf( stderr, "client: failed to connect\n" );
        return SOCKET_FAILED;
    }

    inet_ntop( p->ai_family, get_in_addr( (struct sockaddr *)p->ai_addr ), s, sizeof(s) );
    printf( "client: connecting to %s\n", s );

    freeaddrinfo( servinfo ); // No longer need it

    return sockfd;
}

int nntp_logon( int sockfd, const char *username, const char *password )
{
    int ret;

    ret = nntp_get_reply( sockfd, NNTP_SERVER_READY_POSTING_ALLOWED );

    if ( ret < 0 )
        return ret;

    ret = nntp_send_command( sockfd, NNTP_MORE_AUTHENTICATION_REQUIRED, "AUTHINFO USER %s\r\n", username );

    if ( ret < 0 )
        return ret;

    ret = nntp_send_command( sockfd, NNTP_AUTHENTICATION_SUCCESSFUL, "AUTHINFO PASS %s\r\n", password );

    if ( ret < 0 )
        return ret;

    return 0;
}

int nntp_connect( int *sockfd, const char *hostname, const char *username, const char *password )
{
    *sockfd = socket_open( hostname );

    // Check if the socket opened correctly
    if ( *sockfd == SOCKET_UNKNOWN_HOST )
    {
        printf( "Error opening socket: Unknown host.\n" );
        return CONNECT_FAILURE;
    }
    else if ( *sockfd <= 0 )
        return CONNECT_TRY_AGAIN_LATER;


    // Now let's try to log onto the server
    int ret = nntp_logon( *sockfd, username, password );
    if ( ret == SOCKET_FAILED || ret == SOCKET_EMPTY || ret == SOCKET_TRY_LATER )
    {
        close( *sockfd );
        printf( "Failed to log onto server, trying again in 120 seconds.\n" );
        *sockfd = ret;
        return CONNECT_TRY_AGAIN_LATER;
    }
    else if( ret < 0 )
    {
        close( *sockfd );
        *sockfd = ret;
        printf( "Failed to log onto server, see you~.\n" );

        return CONNECT_FAILURE;
    }

    return 0;
}

int nntp_logoff( int *sockfd )
{
    // And now log off
    int ret = nntp_send_command( *sockfd, NNTP_CONNECTION_CLOSING, "QUIT\r\n" );
    if ( ret < 0 )
        printf( "The server didn't properly say goodbye... The audacity!\n" );

    close( *sockfd );

    return 0;
}

int main( int argc, char **argv )
{
    int *sockfd;
    int n_sockets = 1;
    int i = 0;
    int ret;

    if ( argc != 4 )
    {
        fprintf( stderr, "Usage: sockettest newsserver username password\n" );
        exit( 1 );
    }

    sockfd = malloc( n_sockets * sizeof(int) );

    for( i = 0; i < n_sockets; i++ )
    {
        printf( "*********************** SOCKET %02d *************************\n", i+1 );

        ret = nntp_connect( &sockfd[i], argv[1], argv[2], argv[3] );
        if( ret == CONNECT_FAILURE )
            return -1;
        else if (ret == CONNECT_TRY_AGAIN_LATER )
        {
            // Sleep, try again later
        }

        // Logged on succesfully, let's start posting stuff!
    }

    for( i = 0; i < n_sockets; i++ )
    {
        printf( "*********************** SOCKET %02d *************************\n", i+1 );

        if( sockfd[i] > 0)
            nntp_logoff( &sockfd[i] );
        else
            printf( "Socket %d already closed.\n", i+1 );
    }

    return 0;

}
