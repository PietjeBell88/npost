#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdarg.h>

#include "nntp.h"
#include "socket.h"
#include "common.h"

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

    else if( reply == NNTP_SERVICE_TEMPORARILY_UNAVAILABLE ||
             reply == NNTP_POSTING_FAILED )
    {
        // Usually a connection limit reached
        printf( "NNTP Reply: Service temporarily unavailable.\n" );
        printf( "Received: \"%s\"\n", buffer );
        return SOCKET_TRY_LATER;
    }

    else if( reply == NNTP_SERVER_READY_POSTING_PROHIBITED ||
             reply == NNTP_SERVICE_PERMANENTLY_UNAVAILABLE ||
             reply == NNTP_POSTING_NOT_ALLOWED )
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

int nntp_connect( int *sockfd, const char *hostname, int port, const char *username, const char *password )
{
    *sockfd = socket_open( hostname, port );

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
        // TODO: If we get this error, we should avoid making any new connections
        // e.g. if at the 21st connection the server tells us we reached our connection
        // limit, that 21st connection will fail. However, 22 and beyond will connect fine
        // but might result in problems while posting.
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

int nntp_post( int sockfd, char *buffer, size_t len )
{
    int ret;

    ret = nntp_send_command( sockfd, NNTP_PROCEED_WITH_POST, "POST\r\n" );

    if( ret == SOCKET_CANT_POST )
        return POST_FATAL_ERROR;
    else if( ret < 0 )
        return POST_TRY_LATER;

    // Post
    ret = nntp_sendall( sockfd, buffer, len );
    if( ret < 0 )
        return POST_TRY_LATER;

    // Check if we posted succesfully
    ret = nntp_get_reply( sockfd, NNTP_ARTICLE_POSTED_OK );
    if( ret < 0 )
        return POST_TRY_LATER;

    return 0;
}