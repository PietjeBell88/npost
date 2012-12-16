#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "npost.h"
#include "nntp.h"
#include "common.h"
#include "diskfile.h"

#include "extern/getopt.h"

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
    printf( "  -n, --newsgroup <string,...>    Comma seperated newsgroups to post to. [%s]\n", param->newsgroups[0] );
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
    param->n_newsgroups = 1;
    param->newsgroups = malloc( sizeof(char *) );
    param->newsgroups[0] = "alt.binaries.test";
    param->comment = NULL;
    param->lines = 5000;
    param->linelength = 128;
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

    int b_newsgroup_set = 0;

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
                param->name = strdup( optarg );
                break;
            case 'e':
                param->email = strdup( optarg );
                break;
            case 'n':
                if( !b_newsgroup_set )
                {
                    param->n_newsgroups = 0;
                    free( param->newsgroups );
                    param->newsgroups = NULL;
                }

                param->n_newsgroups++;
                char **old_array = param->newsgroups;
                param->newsgroups = malloc( param->n_newsgroups * sizeof(char *) );
                if( old_array != NULL )
                {
                    memcpy( param->newsgroups, old_array, (param->n_newsgroups - 1) * sizeof(char *) );
                    free( old_array );
                }
                param->newsgroups[param->n_newsgroups - 1] = strdup( optarg );
                b_newsgroup_set = 1;
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

    if( param->n_newsgroups < 1 )
    {
        printf( "Need at least one newsgroup to post to.\n" );
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
        df->filesize = FILESIZE( df->filename ) - df->offset;
    }

    for( int i = 0; i < param->n_newsgroups; i++ )
    {
        printf( "Newsgroup %d: %s\n", i+1, param->newsgroups[i] );
        free( param->newsgroups[i] );
    }

    free( param->newsgroups );

    return 0;
}
int main( int argc, char **argv )
{
    npost_param_t param;

    int ret = npost_parse( argc, argv, &param );

    if ( ret < 0 ) {
        printf( "Error parsing commandline arguments.\n" );
        exit(1);
    }

    exit(0);
    /*

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

    return 0;*/

}
