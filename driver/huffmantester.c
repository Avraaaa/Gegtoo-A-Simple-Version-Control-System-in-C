#include "../src/utils/huffman.h"

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Usage:\n");
        printf("  Compress:   %s -c <input_file> <output_file>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-c") == 0)
    {
        compress(argv[2], argv[3]);
    }
    else
    {
        printf("Invalid option: %s\n", argv[1]);
        printf("Use -c to compress\n");
        return 1;
    }

    return 0;
}
