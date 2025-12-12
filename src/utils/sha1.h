#ifndef SHA1_H
#define SHA1_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define BLOCK_SIZE 64

typedef struct{

    uint32_t H[5];
    uint64_t total_bits;
    uint8_t buffer[64];
    size_t buffer_len;

}Sha1Context;

void sha1_init(Sha1Context *ctx);
void sha1_update(Sha1Context *ctx, const uint8_t *data, size_t len);
void sha1_final(Sha1Context *ctx, uint8_t *digest);
void sha1_print_hash(uint8_t *hash, char *out_string);
void print_hash(const uint8_t *hash);

#endif