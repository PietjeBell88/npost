#ifndef NPOST_DISKFILE_H
#define NPOST_DISKFILE_H

#include <string.h>
#include <stdint.h>

#include "common.h"

typedef struct
{
    char *filename_in;
    char *filename_out;

    size_t offset;     // When splitting
    size_t filesize;   // File size

    crc32_t crc;
} diskfile_t;


size_t read_to_buf( diskfile_t *file, size_t offset, size_t length, char *buf );

#endif