#include <stdio.h>
#include <stdlib.h>

#include "nntp.h"

int main( int argc, char **argv )
{
    int *sockfd;
    int n_sockets = 1;
    int i = 0;
    int ret;

    if ( argc != 5 )
    {
        fprintf( stderr, "Usage: npost newsserver port username password\n" );
        exit( 1 );
    }

    sockfd = malloc( n_sockets * sizeof(int) );

    for( i = 0; i < n_sockets; i++ )
    {
        printf( "*********************** SOCKET %02d *************************\n", i+1 );

        ret = nntp_connect( &sockfd[i], argv[1], atoi(argv[2]), argv[3], argv[4] );
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
