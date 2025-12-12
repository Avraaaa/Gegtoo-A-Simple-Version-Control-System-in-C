#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t K[4] = {0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6};

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define BLOCK_SIZE 64

typedef struct
{

    uint32_t H[5];
    uint64_t total_bits;
    uint8_t buffer[64];
    size_t buffer_len;

} Sha1Context;

void serialize_hash(const uint32_t *H, uint8_t *digest)
{
    digest[0] = (uint8_t)(H[0] >> 24);
    digest[1] = (uint8_t)(H[0] >> 16);
    digest[2] = (uint8_t)(H[0] >> 8);
    digest[3] = (uint8_t)(H[0]);

    digest[4] = (uint8_t)(H[1] >> 24);
    digest[5] = (uint8_t)(H[1] >> 16);
    digest[6] = (uint8_t)(H[1] >> 8);
    digest[7] = (uint8_t)(H[1]);

    digest[8] = (uint8_t)(H[2] >> 24);
    digest[9] = (uint8_t)(H[2] >> 16);
    digest[10] = (uint8_t)(H[2] >> 8);
    digest[11] = (uint8_t)(H[2]);

    digest[12] = (uint8_t)(H[3] >> 24);
    digest[13] = (uint8_t)(H[3] >> 16);
    digest[14] = (uint8_t)(H[3] >> 8);
    digest[15] = (uint8_t)(H[3]);

    digest[16] = (uint8_t)(H[4] >> 24);
    digest[17] = (uint8_t)(H[4] >> 16);
    digest[18] = (uint8_t)(H[4] >> 8);
    digest[19] = (uint8_t)(H[4]);
}

uint32_t helper_function(uint32_t B, uint32_t C, uint32_t D, int n)
{

    if (n == 0)
    {
        return (B & C) | ((~B) & D);
    }
    else if (n == 1)
    {
        return (B ^ C ^ D);
    }
    else if (n == 2)
    {
        return (B & C) | (B & D) | (C & D);
    }
    else
    {
        return (B ^ C ^ D);
    }
}

uint32_t *expand_message_block(const uint8_t *block_512bits, uint32_t W[80])
{

    for (int t = 0; t < 16; t++)
    {
        W[t] = (block_512bits[t * 4] << 24) |
               (block_512bits[t * 4 + 1] << 16) |
               (block_512bits[t * 4 + 2] << 8) |
               (block_512bits[t * 4 + 3]);
    }

    for (int t = 16; t < 80; t++)
    {
        uint32_t temp = W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16];
        W[t] = ROTL(temp, 1);
    }

    return W;
}

void roundcompression(const uint8_t *block_512bits, uint32_t *H)
{
    uint32_t A = H[0];
    uint32_t B = H[1];
    uint32_t C = H[2];
    uint32_t D = H[3];
    uint32_t E = H[4];

    uint32_t W[80];
    expand_message_block(block_512bits, W);

    for (int i = 0; i < 80; i++)
    {
        int t;

        if (i < 20)
            t = 0;
        else if (i < 40)
            t = 1;
        else if (i < 60)
            t = 2;
        else
            t = 3;

        uint32_t temp = ROTL(A, 5) + helper_function(B, C, D, t) + E + K[t] + W[i];

        E = D;
        D = C;
        C = ROTL(B, 30);
        B = A;
        A = temp;
    }

    H[0] = H[0] + A;
    H[1] = H[1] + B;
    H[2] = H[2] + C;
    H[3] = H[3] + D;
    H[4] = H[4] + E;
}

void sha1_init(Sha1Context *ctx)
{

    ctx->H[0] = 0x67452301;
    ctx->H[1] = 0xEFCDAB89;
    ctx->H[2] = 0x98BADCFE;
    ctx->H[3] = 0x10325476;
    ctx->H[4] = 0xC3D2E1F0;

    ctx->total_bits = 0;
    ctx->buffer_len = 0;
}

void sha1_update(Sha1Context *ctx, const uint8_t *data, size_t len)
{

    for (int i = 0; i < len; i++)
    {

        ctx->buffer[ctx->buffer_len++] = data[i];
        ctx->total_bits += 8;

        if (ctx->buffer_len == 64)
        {

            roundcompression(ctx->buffer, ctx->H);
            ctx->buffer_len = 0;
        }
    }
}

void sha1_final(Sha1Context *ctx, uint8_t *digest)
{

    ctx->buffer[ctx->buffer_len++] = 0x80;

    if (ctx->buffer_len > 56)
    {

        while (ctx->buffer_len < 64)
        {
            ctx->buffer[ctx->buffer_len++] = 0;
        }

        roundcompression(ctx->buffer, ctx->H);
        ctx->buffer_len = 0;
    }

    while (ctx->buffer_len < 56)
    {
        ctx->buffer[ctx->buffer_len++] = 0;
    }

    for (int i = 0; i < 8; i++)
    {

        ctx->buffer[56 + i] = (ctx->total_bits >> (56 - (i * 8))) & 0xFF;
    }

    roundcompression(ctx->buffer, ctx->H);

    serialize_hash(ctx->H, digest);
}

void print_hash(const uint8_t *hash)
{

    printf("Hash: ");

    for (int i = 0; i < 20; i++)
    {

        printf("%02x", hash[i]);
    }

    printf("\n");
}

int main()
{
    uint8_t result[20];
    const char *filename = "input.txt";
    FILE *fp = fopen(filename, "rb");

    if (fp == NULL)
    {
        perror("Failed to open file");
        return 1;
    }

    Sha1Context ctx;

    sha1_init(&ctx);

    uint8_t file_buffer[4096];
    size_t bytes_read;

    while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), fp)) != 0)
    {
        sha1_update(&ctx, file_buffer, bytes_read);
    }

    sha1_final(&ctx, result);

    fclose(fp);

    print_hash(result);

    return 0;
}