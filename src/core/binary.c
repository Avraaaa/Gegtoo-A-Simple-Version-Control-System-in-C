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

#include "../../include/core/binary.h"

void hex_to_binary(const char *hex, unsigned char *out)
{

    for (int i = 0; i < 20; i++)
    {
        sscanf(hex + 2 * i, "%2hhx", &out[i]);
    }
}


void pack32_be(uint32_t b, unsigned char *u)
{

    u[0] = (b >> 24) & 0xFF;
    u[1] = (b >> 16) & 0xFF;
    u[2] = (b >> 8) & 0xFF;
    u[3] = (b) & 0xFF;
}


void pack16_be(uint16_t b, unsigned char *u)
{

    u[0] = (b >> 8) & 0xFF;
    u[1] = (b) & 0xFF;
}


uint32_t unpack32_be(const unsigned char *b)
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}


uint16_t unpack16_be(const unsigned char *b)
{
    return ((uint16_t)b[0] << 8) | b[1];
}


