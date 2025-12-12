#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define BLOCK_SIZE 64


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
    pad_blocks("abc");

    pad_blocks("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

    return 0;
}