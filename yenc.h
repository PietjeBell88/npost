#ifndef NPOST_YENC_H
#define NPOST_YENC_H

#include "common.h"
#include "diskfile.h"

#define YENC_LINE_LENGTH 128

size_t yenc_buffer_size( int lines );

long yenc_encode( diskfile_t *df, int part, int parts, int lines, char *outbuf, size_t psize );

#endif
