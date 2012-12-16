#ifndef NPOST_COMMON_H
#define NPOST_COMMON_H

#include <string.h>

#define MIN(a,b) ({ \
    a > b ? b : a; \
})

size_t FILESIZE(char *fname);

char *strdup2( const char * s );

#endif