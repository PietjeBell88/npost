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

#define NNTP_SERVER_READY_POSTING_ALLOWED 200
#define NNTP_MORE_AUTHENTICATION_REQUIRED 381
#define NNTP_AUTHENTICATION_SUCCESSFUL 281
#define NNTP_CONNECTION_CLOSING 205

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int nntp_get_reply( int sockfd, int code )
{
    char buffer[256];
    int reply = 0;
    int n;

    while( reply != code )
    {
        n = recv( sockfd, buffer, 255, 0 );
        if ( n == -1 )
            break;

        buffer[n] = '\0';

        printf( "Reply: %s\n", buffer );
        int reply = atoi( buffer );
        printf( "Reply Code: %d\n", reply );

        if ( reply == code )
            break;
    }
    return n==-1?-1:0;
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
        return -1;

    if( total == len )
        return 1;

    return 0;
}

int nntp_send_command( int sockfd, int code, const char *command, ... )
{
    char buffer[256];
    size_t len;
    int ret;


    va_list args;
    va_start( args, command );
    len = vsprintf( buffer, command, args );
    va_end( args );

    printf( "Sending command: %s\n", buffer );
    ret = nntp_sendall( sockfd, buffer, len );
    if (ret >= 0)
        ret = nntp_get_reply( sockfd, code );

    return ret;
}

int main( int argc, char **argv )
{
    int sockfd; //, numbytes;
    //char buf[MAXDATASIZE];
    int rv;
    char s[INET6_ADDRSTRLEN];
    char ipstr[INET6_ADDRSTRLEN];

    struct addrinfo hints, *servinfo, *p;

    if ( argc != 4 )
    {
        fprintf( stderr, "Usage: sockettest newsserver username password\n" );
        exit( 1 );
    }

    memset( &hints, 0, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo( argv[1], PORT, &hints, &servinfo )) != 0)
    {
        fprintf( stderr, "getaddrinfo: %s\n", gai_strerror(rv) );
        return 1;
    }

    printf("IP addresses for %s:\n\n", argv[1]);

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
        return 2;
    }

    inet_ntop( p->ai_family, get_in_addr( (struct sockaddr *)p->ai_addr ), s, sizeof(s) );
    printf( "client: connecting to %s\n", s );

    freeaddrinfo( servinfo ); // No longer need it

    /* Now try to login */
    int login_success = 0;
    if( (login_success = nntp_get_reply( sockfd, NNTP_SERVER_READY_POSTING_ALLOWED )) == -1 )
        exit( 1 );
    login_success &= nntp_send_command( sockfd, NNTP_MORE_AUTHENTICATION_REQUIRED, "AUTHINFO USER %s\r\n", argv[2] );
    login_success &= nntp_send_command( sockfd, NNTP_AUTHENTICATION_SUCCESSFUL, "AUTHINFO PASS %s\r\n", argv[3] );

    // And now log off
    nntp_send_command( sockfd, NNTP_CONNECTION_CLOSING, "QUIT\r\n" );
    /*if( (numbytes = recv( sockfd, buf, MAXDATASIZE-1, 0 )) == -1 )
    {
        perror( "recv" );
        exit( 1 );
    }

    buf[numbytes] = '\0';

    printf( "client: received '%s'\n", buf );*/

    close( sockfd );

    return 0;

}
