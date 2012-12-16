#include <stdlib.h>
#include <stdio.h>

#include "diskfile.h"
#include "yenc.h"
#include "extern/crc32.h"

static long yenc_data( char *inbuf, char *outbuf, size_t psize, crc32_t *crc );

size_t yenc_buffer_size( int lines )
{
    /* Worst-case for yenc is twice the original size, plus
    * the line endings; though realistically it will be only
    * a few percent bigger than the original.
    * Add 512 to cover ybegin, ypart and yend lines. */

    size_t message_size = YENC_LINE_LENGTH * lines;
    return ((message_size * 2) + ((message_size / YENC_LINE_LENGTH) * 4) + 512);
}

long yenc_encode( diskfile_t *df, int part, int parts, int lines, char *outbuf, size_t psize )
{
    // Aliases
    char *filename = df->filename;
    size_t filesize = df->filesize;

    // Index
    char *pi = outbuf;

    // Output buffer
    size_t message_size = YENC_LINE_LENGTH * lines;
    char *inbuf = malloc( message_size );

    /* and encode */
    crc32_t crc = 0;

    size_t pbegin = message_size * (part - 1) + 1;
    size_t pend = pbegin + psize - 1;
    read_to_buf( df, pbegin - 1, psize, inbuf );

    /* The first line (or two) */
    if ( parts == 1 )
        pi += sprintf( pi, "=ybegin line=%i size=%li name=%s\r\n",
                       YENC_LINE_LENGTH, filesize, filename );
    else
    {
        // Header
        pi += sprintf( pi, "=ybegin part=%i total=%i line=%i size=%li name=%s\r\n",
                       part, parts, YENC_LINE_LENGTH, filesize, filename ); // filename is basename

        pi += sprintf( pi, "=ypart begin=%li end=%li\r\n",
                           pbegin, pend );
    }

    // Encoded Data

    pi += yenc_data( inbuf, outbuf, psize, &crc );

    // Footer
    if( parts == 1 )
        pi += sprintf(pi, "=yend size=%li crc32=%08x\r\n",
                psize, crc);
    else
        pi += sprintf(pi, "=yend size=%li part=%i pcrc32=%08x crc32=%08x\r\n",
                psize, part, crc, df->crc);

    return pi - outbuf;
}

long yenc_data( char *inbuf, char *outbuf, size_t psize, crc32_t *crc )
{
    size_t counter;
    int ylinepos;
    unsigned char *p, *ch, c;

    ylinepos = 0;
    counter = 0;

    ch = (unsigned char *) outbuf;
    p = (unsigned char *) inbuf;

    crc32( (char *) inbuf, psize, crc );

    while (counter < psize) {
        if (ylinepos >= YENC_LINE_LENGTH)
        {
            *ch++ = '\r';
            *ch++ = '\n';
            ylinepos = 0;
        }

        c = *p + 42;

        switch( c )
        {
            case '.':
                if (ylinepos > 0)
                    break;
            case '\0':
            case 9:
            case '\n':
            case '\r':
            case '=':
                *ch++ = '=';
                c += 64;
                ylinepos++;
        }

        *ch++ = c;

        counter++;
        ylinepos++;
        p++;
    }

    *ch++ = '\r';
    *ch++ = '\n';
    *ch = '\0';

    return ch - (unsigned char *) outbuf;
}
