#ifndef CRC32_H
#define CRC32_H

void crc32(const void *data, size_t n_bytes, uint32_t* crc);

void crc32_file(char *filename, uint32_t *crc);

#endif