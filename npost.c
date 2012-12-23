#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>

#include "npost.h"
#include "nntp.h"
#include "yenc.h"
#include "common.h"
#include "diskfile.h"

#include "extern/getopt.h"
#include "extern/crc32.h"

#define SLEEP_TIME 30

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
    printf( "  -x, --split <long>,             Split each input file every <long> bytes. [%ld]\n", param->split );
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
    { "split",      required_argument, NULL, 'x' },
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
    param->split = 0;

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
            case 'x':
                param->split = atol( optarg );
                switch( optarg[strlen(optarg) - 1] )
                {
                    case 'K':
                        param->split *= 1000;
                        break;
                    case 'k':
                        param->split <<= 10;
                        break;
                    case 'M':
                        param->split *= 1000000;
                        break;
                    case 'm':
                        param->split <<= 20;
                        break;
                    case 'G':
                        param->split *= 1000000000;
                        break;
                    case 'g':
                        param->split <<= 30;
                        break;
                    default:
                        break;
                }
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

    if( param->split != 0 && param->split < (1 << 10) )
    {
        printf( "--split: Can't split smaller than 1kB.\n" );
        b_error |= 1;
    }

    if( b_error )
        return -1;

    if ( param->threads < 1 )
        param->threads = 1;

    // First open all files, check if they exist, and get their filesize
    param->input_files = malloc( param->n_input_files * sizeof(diskfile_t) );

    for ( int i = 0; i < param->n_input_files; i++ )
    {
        diskfile_t *df = &param->input_files[i];

        df->filename_in = strdup( argv[optind++] );
        df->filename_out = strdup( df->filename_in );
        df->offset   = 0;
        df->filesize = FILESIZE( df->filename_in );
        if( df->filesize == 0 )
        {
            printf( "Error opening file: %s\n", df->filename_in );
            return -1;
        }
        df->filesize = df->filesize - df->offset;
    }

    // Set the comment to be the filename if the comment was unset
    if( param->comment == NULL && param->n_input_files == 1 )
        param->comment = strdup( param->input_files[0].filename_in );

    // Now count how many virtual diskfiles we need if we split
    if( param->split )
    {
        param->n_split_files = 0;

        for( int i = 0; i < param->n_input_files; i++ )
        {
            diskfile_t *df = &param->input_files[i];
            param->n_split_files += (df->filesize + param->split - 1) / param->split;
        }
    }
    else
        param->n_split_files = param->n_input_files;

    // Generate the list of virtual files
    param->split_files = malloc( param->n_split_files * sizeof(diskfile_t) );

    for( int i = 0, t = 0; i < param->n_input_files; i++ )
    {
        diskfile_t *df_in  = &param->input_files[i];

        // Else we start splitting~:
        char split_fnformat[100];
        int split_parts    = param->split ? (df_in->filesize + param->split - 1) / param->split : 1;
        int split_digits   = snprintf( 0, 0, "%d", split_parts );
        int split_fnlength = strlen( df_in->filename_out ) + split_digits + 2; // one dot, and one terminating zero

        sprintf( split_fnformat, "%%s.%%0%dd", split_digits );

        for( int j = 0; j < split_parts; j++ )
        {
            diskfile_t *df_out = &param->split_files[t++];

            // If we don't have to split, just copy stuff
            if( !param->split || split_parts == 1 )
            {
                memcpy( df_out, df_in, sizeof(diskfile_t) );
                df_out->filename_in = strdup( df_in->filename_in );
                df_out->filename_out = strdup( df_in->filename_out );
                continue;
            }

            df_out->filename_in = strdup( df_in->filename_in );
            df_out->filename_out = malloc( split_fnlength );
            snprintf( df_out->filename_out, split_fnlength, split_fnformat, df_in->filename_out, j+1 );

            df_out->offset = j * param->split;

            df_out->filesize = MIN(df_in->filesize - df_out->offset, param->split);
        }
    }

    return 0;
}

size_t npost_get_part( npost_param_t *param, int filenum, int partnum, char **outbuf )
{
    // Alias
    diskfile_t *df = &param->split_files[filenum];
    size_t blocksize = YENC_LINE_LENGTH * param->lines;
    int parts = (df->filesize + blocksize - 1) / blocksize;

    // Allocate buffers
    char *inbuf = malloc( blocksize );
    char *buffer = malloc( yenc_buffer_size( param->lines ) + 1024 ); // message-id also takes up some space
    char *buffer_orig = buffer;

    // Generate the subject
    char subject[256];

    sprintf( subject, "%s - [%d/%d] - \"%s\" yEnc (%d/%d)",
             param->comment, filenum+1, param->n_split_files, df->filename_out, partnum+1, parts);

    // Generate the message id
    char message_id[1024];
    srand( time( NULL ) );
    sprintf( message_id, "Part%dof%d.%08X%08X%08X%d@8934785.local",
             partnum+1, parts, rand() % 0xffffffff, rand() % 0xffffffff, rand() % 0xffffffff, (int) time( NULL ) );

    buffer += sprintf( buffer, "From: %s (%s)\r\nNewsgroups: %s\r\nSubject: %s\r\nMessage-ID: <%s>\r\n\r\n",
                       param->email, param->name, param->newsgroups, subject, message_id );


    size_t psize = read_to_buf( df, partnum * blocksize, blocksize, inbuf );

    buffer += yenc_encode( df->filename_out, df->filesize, df->crc, inbuf,
                           partnum+1, parts, param->lines, psize, buffer );

    buffer += sprintf( buffer, "\r\n.\r\n" );

    *outbuf = buffer_orig;

    free( inbuf );

    return buffer - buffer_orig;
}

int conditional_sleep( npost_t *h )
{
    struct timespec   ts;
    struct timeval    tp;

    int ret;

    gettimeofday( &tp, NULL );

    /* Convert from timeval to timespec */
    ts.tv_sec  = tp.tv_sec;
    ts.tv_nsec = tp.tv_usec * 1000;
    ts.tv_sec += SLEEP_TIME;

    pthread_mutex_lock( h->mut );

    while( h->head != NULL ) {
        ret = pthread_cond_timedwait( h->cond_list_empty, h->mut, &ts );
        if( ret == ETIMEDOUT || ret == 0 )
            break;
    }

    pthread_mutex_unlock( h->mut );

    return ret;
}

void * poster_thread( void *arg )
{
    npost_t *h = (npost_t *)arg;

    // Allocate buffers
    char *buffer;
    int ret;
    npost_item_t *item;

    int sockfd = -1;

    while( 1 )
    {
        // Connect when necessary
        while( sockfd <= 0 )
        {
            ret = nntp_connect( &sockfd, h->param.server, h->param.port, h->param.username, h->param.password );
            if( ret == CONNECT_FAILURE )
                return NULL;
            else if (ret == CONNECT_TRY_AGAIN_LATER )
            {
                ret = conditional_sleep( h );
                if( ret == 0 ) // Article list is empty
                    return NULL;
            }
        }

        // Get an article to post
        pthread_mutex_lock(h->mut);
        item = h->head;

        if( h->head == NULL )
        {
            // We're done
            pthread_cond_broadcast( h->cond_list_empty );
            pthread_mutex_unlock(h->mut);
            break;
        }

        h->head = h->head->next;
        pthread_mutex_unlock(h->mut);

        // Post the article
        printf( "**** Posting! sockfd: %2d, filenum: %2d, partnum: %3d *****\n", sockfd, item->filenum, item->partnum );

        size_t bytes_to_post = npost_get_part( &h->param, item->filenum, item->partnum, &buffer );

        ret = nntp_post( sockfd, buffer, bytes_to_post );

        free( buffer );

        if( ret < 0 )
        {
            // Put the article back in the queue as something went wrong
            item->next = NULL;
            pthread_mutex_lock(h->mut);

            if( h->head == NULL )
                h->head = item;
            else
                h->tail->next = item;

            h->tail = item;

            pthread_mutex_unlock(h->mut);
        }

        // Depending on what went wrong, we either try again later, or kill ourselves
        if( ret == POST_TRY_LATER )
        {
            nntp_logoff( tid, &sockfd );
            int retval = conditional_sleep( h );
            if( retval == 0 ) // Article list is empty
                return NULL;
        }
        else if ( ret == POST_FATAL_ERROR )
            return NULL;

        // If posting went succesfully, free this item
        if( ret == 0 )
            free( item );
    }

    nntp_logoff( &sockfd );

    return NULL;
}

void crc32_file( diskfile_t *df )
{
    char buf[1L << 15];
    df->crc = 0;
    size_t bytes_read;
    size_t offset;

    while( (bytes_read = read_to_buf( df, offset, sizeof(buf), buf )) > 0 )
    {
        crc32( buf, bytes_read, &df->crc );
        offset += bytes_read;
    }
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
    signal( SIGPIPE, SIG_IGN );
    ret = nntp_connect( &testfd, param.server, param.port, param.username, param.password );
    if( ret == CONNECT_FAILURE )
    {
        printf( "Failed to make even a single connection! Exiting...\n" );
        return -1;
    }
    nntp_logoff( &testfd );

    // Initialize a handle to be used by the threads
    npost_t h;

    memcpy( &h, &param, sizeof(npost_param_t) );
    h.threads = NULL;
    h.head = NULL;
    h.tail = NULL;
    h.mut = malloc( sizeof(pthread_mutex_t) );
    pthread_mutex_init( h.mut, NULL);

    h.cond_list_empty = malloc( sizeof(pthread_cond_t) );
    pthread_cond_init( h.cond_list_empty, NULL );

    // Let's add parts to the queue!
    size_t blocksize = YENC_LINE_LENGTH * param.lines;
    for( int f = 0; f < param.n_split_files; f++ )
    {
        diskfile_t *df = &param.split_files[f];
        crc32_file( df );
        int parts = (param.split_files[f].filesize + blocksize - 1) / blocksize;

        for( int p = 0; p < parts; p++ )
        {
            if( h.head == NULL )
            {
                h.head = malloc( sizeof(npost_item_t) );
                h.tail = h.head;
            }
            else
            {
                h.tail->next = malloc( sizeof(npost_item_t) );
                h.tail = h.tail->next;
            }

            h.tail->filenum = f;
            h.tail->partnum = p;
            h.tail->next = NULL;
        }
    }

    // Initialize the threads, and give each thread its own copy of the handle
    h.threads = malloc( param.threads * sizeof(pthread_t) );

    for ( int i = 0; i < param.threads; i++ )
        pthread_create( &h.threads[i], NULL, poster_thread, &h );

    for( int i = 0; i < param.threads; i++ )
        pthread_join( h.threads[i], NULL );

    // free all the mallocs!
    free( h.threads );

    pthread_mutex_destroy( h.mut );
    free( h.mut );

    pthread_cond_destroy( h.cond_list_empty );
    free( h.cond_list_empty );

    free( param.server );
    free( param.username );
    free( param.password );
    free( param.name );
    free( param.email );
    free( param.newsgroups );
    free( param.comment );

    for ( int i = 0; i < param.n_input_files; i++ )
    {
        free( param.input_files[i].filename_in );
        free( param.input_files[i].filename_out );
    }
    free( param.input_files );

    for ( int i = 0; i < param.n_split_files; i++ )
    {
        free( param.split_files[i].filename_in );
        free( param.split_files[i].filename_out );
    }
    free( param.split_files );

    return 0;

}
