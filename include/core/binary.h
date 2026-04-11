#ifndef CORE_BINARY_H
#define CORE_BINARY_H

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

void hex_to_binary(const char *hex, unsigned char *out);
void pack32_be(uint32_t b, unsigned char *u);
void pack16_be(uint16_t b, unsigned char *u);
uint32_t unpack32_be(const unsigned char *b);
uint16_t unpack16_be(const unsigned char *b);

#endif
