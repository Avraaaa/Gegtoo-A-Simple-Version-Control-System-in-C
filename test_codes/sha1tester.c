#include "sha1.h"

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

    while((bytes_read = fread(file_buffer,1,sizeof(file_buffer),fp))!=0){
        sha1_update(&ctx, file_buffer, bytes_read);
    }

    sha1_final(&ctx,result);

    fclose(fp);

    print_hash(result);

    return 0;
}
