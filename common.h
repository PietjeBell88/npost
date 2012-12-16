#ifndef NPOST_COMMON_H
#define NPOST_COMMON_H

#include <stdint.h>
#include <string.h>

#define MIN(a,b) ({ \
    a > b ? b : a; \
})

typedef uint32_t crc32_t;

size_t FILESIZE(char *fname);

char *strdup2( const char * s );

#endif