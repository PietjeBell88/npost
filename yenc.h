#ifndef NPOST_YENC_H
#define NPOST_YENC_H

#include "common.h"
#include "diskfile.h"

#define YENC_LINE_LENGTH 128

size_t yenc_buffer_size( int lines );

long yenc_encode( char *filename, size_t filesize, crc32_t crcfile, char *inbuf,
                  int part, int parts, int lines, size_t psize, char *outbuf );

#endif
