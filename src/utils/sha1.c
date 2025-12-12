#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define BLOCK_SIZE 64

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))


void expand_message_block(const uint8_t *block_512bits)
{
    uint32_t W[80]; 

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

    printf("Expansion Complete.\n");
    for(int i = 0;i<80;i++){

        printf("W[%d], %08X\n",i,W[i]);

    }
}



void pad_blocks(const char *input_string)
{

    uint64_t original_byte_len = strlen(input_string);
    uint64_t original_bit_len = original_byte_len * 8;

    int total_len = original_byte_len + 1 + 8;
    int num_blocks = (total_len + (BLOCK_SIZE - 1)) / BLOCK_SIZE;

    printf("Original String: \"%s\"\n", input_string);
    printf("Length: %llu bytes (%llu bits)\n", original_byte_len, original_bit_len);
    printf("Padding into %d block(s)...\n\n", num_blocks);

    uint8_t *padded_message = calloc(num_blocks * BLOCK_SIZE, 1);

    memcpy(padded_message, input_string, original_byte_len);

    padded_message[original_byte_len] = 0x80;

    int end_pos = (num_blocks * BLOCK_SIZE) - 8;

    for (int i = 0; i < 8; i++)
    {
        padded_message[end_pos + i] = (original_bit_len >> (56 - (i * 8))) & 0xFF;
    }

    for (int block_num = 0; block_num < num_blocks; block_num++)
    {

        printf("Block %d:\n", block_num + 1);

        uint8_t *block = &padded_message[block_num * BLOCK_SIZE];

        for (int i = 0; i < BLOCK_SIZE; i++)
        {

            printf("%02X ", block[i]);

            if ((i + 1) % 16 == 0)
                printf("\n");
        }

        printf("\n");
    }

    free(padded_message);
}


int main()
{
    expand_message_block("abc");

    printf("2nd string");

    expand_message_block("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

    return 0;
}

