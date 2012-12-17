#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "npost.h"
#include "nntp.h"
#include "yenc.h"
#include "common.h"
#include "diskfile.h"

#include "extern/getopt.h"
#include "extern/crc32.h"

static void npost_print_version()
{
    printf( "npost version: \"%s\"\n\n", NPOST_VERSION );
    printf( "It's like newspost but with less features:\n" );
    printf( " - Only supports posting binary files.\n" );
    printf( " - No more uuencode.\n" );
    printf( " - Fixed format for multiple-file posts.\n\n" );
    printf( "Made possible by \"Bad Economy\"^TM.\n" );
}

static void help( npost_param_t *param )
{
    printf( "npost version: \"%s\"\n\n", NPOST_VERSION );
    printf( "Usage: npost [options] [files]\n\n" );
    printf( "Options:\n" );
    printf( "  -h, --help                      Print this message\n" );
    printf( "  -v, --version                   Return the version/program info\n" );
    printf( "  -s, --server <string>           Hostname or IP of the news server\n" );
    printf( "  -o, --port                      Return the version/program info\n" );
    printf( "  -u, --user <string>             Username on the news server\n" );
    printf( "  -p, --password <string>,        Password on the news server\n" );
    printf( "  -t, --threads <int>,            Amount of threads to use for posting [%d]\n", param->threads );
    printf( "  -f, --name <string>,            Your full name. [%s]\n", param->name );
    printf( "  -e, --email <string>,           Your email address [%s]\n", param ->email );
    printf( "  -n, --newsgroup <string,...>    Comma seperated newsgroups to post to. [%s]\n", param->newsgroups );
    printf( "  -c, --comment <string>,         Subject of the post. Default is equal to filename.\n" );
    printf( "  -l, --lines <int>,              Amount of lines to post with [%d]\n", param->lines ) ;
}

static char short_options[] = "c:e:f:hl:n:o:p:s:t:u:y:";
static struct option long_options[] =
{
    { "help",             no_argument, NULL, 'h' },
    { "version",          no_argument, NULL, 'v' },
    { "server",     required_argument, NULL, 's' },
    { "port",       required_argument, NULL, 'o' },
    { "user",       required_argument, NULL, 'u' },
    { "password",   required_argument, NULL, 'p' },
    { "threads",    required_argument, NULL, 't' },
    { "name",       required_argument, NULL, 'f' },
    { "email",      required_argument, NULL, 'e' },
    { "newsgroup",  required_argument, NULL, 'n' },
    { "comment",    required_argument, NULL, 'c' },
    { "lines",      required_argument, NULL, 'l' },
    {0, 0, 0, 0}
};

void npost_param_default( npost_param_t *param )
{
    // Defaults
    param->server = NULL;
    param->port = 119;
    param->username = NULL;
    param->password = NULL;
    param->threads = 1;
    param->name = "Anonymous";
    param->email= "anonymous@anonymous.org";
    param->newsgroups = "alt.binaries.test";
    param->comment = NULL;
    param->lines = 5000;
    param->linelength = 128;

    // Because otherwise it's hard to know if we can free() it or not
    param->name = strdup( param->name );
    param->email = strdup( param->email );
    param->newsgroups = strdup( param->newsgroups );
}

int npost_parse( int argc, char **argv, npost_param_t *param )
{

    npost_param_default( param );

    // First check for help
    if ( argc == 1 )
    {
        help( param );
        exit( 0 );
    }
    for( optind = 0;; )
    {
        int c = getopt_long( argc, argv, short_options, long_options, NULL );

        if( c == -1 )
            break;
        else if( c == 'h' )
        {
            help( param );
            exit(0);
        }
    }

    // Check for other options
    for( optind = 0;; )
    {
        int c = getopt_long( argc, argv, short_options, long_options, NULL );

        if( c == -1 )
            break;

        switch( c )
        {
            case 'v':
                npost_print_version();
                exit(0);
            case 's':
                param->server = strdup( optarg );
                break;
            case 'o':
                param->port = atoi( optarg );
                break;
            case 'u':
                param->username = strdup( optarg );
                break;
            case 'p':
                param->password = strdup( optarg );
                break;
            case 't':
                param->threads = atoi( optarg );
                break;
            case 'f':
                free( param->name );
                param->name = strdup( optarg );
                break;
            case 'e':
                free( param->email );
                param->email = strdup( optarg );
                break;
            case 'n':
                free( param->newsgroups );
                param->newsgroups = strdup( optarg );
                break;
            case 'c':
                param->comment = strdup( optarg );
                break;
            case 'l':
                param->lines = atoi( optarg );
                break;
            default:
                printf( "Error: getopt returned character code 0%o = %c\n", c, c );
                return -1;
        }
    }

    if ( optind < argc )
        param->n_input_files = argc - optind;


    // Parameter checking
    int b_error = 0;
    if( param->n_input_files < 1 )
    {
        printf( "You have to give me a file to post.\n" );
        b_error |= 1;
    }

    if( param->comment == NULL && param->n_input_files > 1 )
    {
        printf( "You have to specify the comment when posting more than one file.\n" );
        b_error |= 1;
    }

    if( param->server == NULL )
    {
        printf( "--server is required.\n" );
        b_error |= 1;
    }

    if( param->username == NULL )
    {
        printf( "--user is required.\n" );
        b_error |= 1;
    }

    if( param->password == NULL )
    {
        printf( "--password is required.\n" );
        b_error |= 1;
    }

    if( b_error )
        return -1;

    if ( param->threads < 1 )
        param->threads = 1;

    param->input_files = malloc( param->n_input_files * sizeof(diskfile_t) );

    for ( int i = 0; i < param->n_input_files; i++ )
    {
        diskfile_t *df = &param->input_files[i];

        df->filename = strdup( argv[optind++] );
        df->offset   = 0; //offsets[i];
        df->filesize = FILESIZE( df->filename );
        if( df->filesize == 0 )
        {
            printf( "Error opening file: %s\n", df->filename );
            return -1;
        }
        df->filesize = df->filesize - df->offset;
    }

    // Set the comment to be the filename if the comment was unset
    if( param->comment == NULL && param->n_input_files == 1 )
        param->comment = strdup( param->input_files[0].filename );

    return 0;
}

size_t npost_get_part( npost_param_t *param, int filenum, int partnum, char **outbuf )
{
    // Alias
    diskfile_t *df = &param->input_files[filenum];
    size_t blocksize = YENC_LINE_LENGTH * param->lines;
    int parts = (df->filesize + blocksize - 1) / blocksize;

    // Allocate buffers
    char *inbuf = malloc( blocksize );
    char *buffer = malloc( yenc_buffer_size( param->lines ) + 1024 ); // message-id also takes up some space
    char *buffer_orig = buffer;

    // Generate the subject
    char subject[256];

    sprintf( subject, "%s - [%d/%d] - \"%s\" yEnc (%d/%d)",
             param->comment, filenum+1, param->n_input_files, df->filename, partnum+1, parts);

    // Generate the message id
    char message_id[1024];
    srand( time( NULL ) );
    sprintf( message_id, "Part%dof%d.%08X%08X%08X%d@8934785.local",
             partnum+1, parts, rand() % 0xffffffff, rand() % 0xffffffff, rand() % 0xffffffff, (int) time( NULL ) );

    buffer += sprintf( buffer, "From: %s (%s)\r\nNewsgroups: %s\r\nSubject: %s\r\nMessage-ID: <%s>\r\n\r\n",
                       param->email, param->name, param->newsgroups, subject, message_id );


    *buffer = '\0';
    printf( "*************** MESSAGE HEADER *********:\n%s\n", buffer_orig );

    size_t psize = read_to_buf( df, partnum * blocksize, blocksize, inbuf );

    buffer += yenc_encode( df->filename, df->filesize, df->crc, inbuf,
                           partnum+1, parts, param->lines, psize, buffer );

    buffer += sprintf( buffer, "\r\n.\r\n" );

    *outbuf = buffer_orig;

    free( inbuf );

    return buffer - buffer_orig;
}


int main( int argc, char **argv )
{
    npost_param_t param;

    int ret = npost_parse( argc, argv, &param );

    if ( ret < 0 ) {
        printf( "Error parsing commandline arguments.\n" );
        exit(1);
    }

    // First try to make just one connection, and see if it fails
    int testfd = 0;
    ret = nntp_connect( &testfd, param.server, param.port, param.username, param.password );
    if( ret == CONNECT_FAILURE )
    {
        printf( "Failed to make even a single connection! Exiting...\n" );
        return -1;
    }
    nntp_logoff( &testfd );

    // Succeeded in making a single connection, now let's get going~!
    int *sockfd;
    sockfd = malloc( param.threads * sizeof(int) );

    for( int i = 0; i < param.threads; i++ )
    {
        ret = nntp_connect( &sockfd[i], param.server, param.port, param.username, param.password );
        if( ret == CONNECT_FAILURE )
            return -1;
        else if (ret == CONNECT_TRY_AGAIN_LATER )
        {
            // Sleep, try again later
        }
    }

    // Logged on succesfully, let's start posting stuff!
    size_t blocksize = YENC_LINE_LENGTH * param.lines;

    // TODO: posting with more than one thread
    for( int f = 0; f < param.n_input_files; f++ )
    {
        diskfile_t *df = &param.input_files[f];
        crc32_file( df->filename, &df->crc );
        int parts = (param.input_files[f].filesize + blocksize - 1) / blocksize;

        for( int p = 0; p < parts; p++ )
        {
            // Allocate buffers
            char *buffer;

            size_t bytes_to_post = npost_get_part( &param, f, p, &buffer );

            ret = nntp_post( sockfd[0], buffer, bytes_to_post );

            free( buffer );

            if( ret == POST_TRY_LATER )
            {
                // TODO: sleep and properly reconnect/check return values
                nntp_logoff( &sockfd[0] );
//              nntp_connect()
            }
            else if ( ret == POST_FATAL_ERROR )
                exit( 2 );
        }
    }


    for( int i = 0; i < param.threads; i++ )
    {

        printf( "*********************** SOCKET %02d *************************\n", i+1 );

        if( sockfd[i] > 0)
            nntp_logoff( &sockfd[i] );
        else
            printf( "Socket %d already closed.\n", i+1 );
    }

    // free all the mallocs!
    free( sockfd );


    free( param.server );
    free( param.username );
    free( param.password );
    free( param.name );
    free( param.email );
    free( param.newsgroups );
    free( param.comment );

    for ( int i = 0; i < param.n_input_files; i++ )
        free( param.input_files[i].filename );
    free( param.input_files );
    return 0;

}
