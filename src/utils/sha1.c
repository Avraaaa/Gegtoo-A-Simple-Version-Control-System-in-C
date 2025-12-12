#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define BLOCK_SIZE 64

uint32_t K[4] = {0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6};

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

void roundcompression(const uint8_t *block_512bits, uint8_t *result, uint32_t *H)
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

        if (i >= 0 && i < 20)
            t = 0;
        else if (i >= 20 && i < 40)
            t = 1;
        else if (i >= 40 && i < 60)
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

    serialize_hash(H, result);
}


int pad_blocks(const char *input_string, uint8_t **padded_message)
{

    uint64_t original_byte_len = strlen(input_string);
    uint64_t original_bit_len = original_byte_len * 8;

    int total_len = original_byte_len + 1 + 8;
    int num_blocks = (total_len + (BLOCK_SIZE - 1)) / BLOCK_SIZE;

    printf("Original String: \"%s\"\n", input_string);
    printf("Length: %llu bytes (%llu bits)\n", original_byte_len, original_bit_len);
    printf("Padding into %d block(s)...\n\n", num_blocks);

    *padded_message = calloc(num_blocks * BLOCK_SIZE, 1);

    memcpy(*padded_message, input_string, original_byte_len);

    (*padded_message)[original_byte_len] = 0x80;

    int end_pos = (num_blocks * BLOCK_SIZE) - 8;

    for (int i = 0; i < 8; i++)
    {
        (*padded_message)[end_pos + i] = (original_bit_len >> (56 - (i * 8))) & 0xFF;
    }

    return num_blocks;
}

void sha1(const char *input_string, size_t string_len, uint8_t *result)
{
    uint32_t H[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};

    uint8_t *padded_message;
    int num_blocks;

    num_blocks = pad_blocks(input_string, &padded_message);

    for (int i = 0; i < num_blocks; i++)
    {
        roundcompression(&padded_message[i * BLOCK_SIZE], result, H);
    }

    free(padded_message);
}


void print_hash(const uint8_t *hash){

    printf("Hash: ");

    for(int i = 0;i<20;i++){

        printf("%02x",hash[i]);

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

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    rewind(fp);

    char *input_buffer = malloc(fsize);
    if (input_buffer == NULL)
    {
        perror("Failed to allocate memory");
        fclose(fp);
        return 1;
    }

    size_t bytes_read = fread(input_buffer, 1, fsize, fp);
    fclose(fp);

    sha1(input_buffer, bytes_read, result);

    print_hash(result);

    free(input_buffer);
    return 0;
}