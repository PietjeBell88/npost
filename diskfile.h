#ifndef NPOST_DISKFILE_H
#define NPOST_DISKFILE_H

#include <string.h>
#include <stdint.h>

typedef struct
{
    char *filename;

    size_t offset;     // When splitting
    size_t filesize;   // File size
} diskfile_t;


size_t read_to_buf( diskfile_t *file, long int offset, size_t length, char *buf );

#endif