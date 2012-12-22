#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "socket.h"

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int socket_open( const char *hostname, int port )
{
    int sockfd; //, numbytes;
    int rv;
    char s[INET6_ADDRSTRLEN];
    char ipstr[INET6_ADDRSTRLEN];
    char charport[6];

    struct addrinfo hints, *servinfo, *p;

    sprintf( charport, "%d", port );

    memset( &hints, 0, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo( hostname, charport, &hints, &servinfo )) != 0)
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

    // Set read and write timeout
    struct timeval timeout;
    timeout.tv_sec = 20;
    timeout.tv_usec = 0;

    setsockopt( sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout) );
    setsockopt( sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout) );

    return sockfd;
}
