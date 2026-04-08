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
#include "../../include/core/index.h"
#include "../../include/utils/sha1.h"
#include "../../include/commands.h"
#include "../../include/core/tree.h"

void geg_remove(int argc, char *argv[])
{

    if (argc < 3)
    {
        printf("Usage: ./geg remove [--cached] <files...>\n");
        return;
    }

    int cached = 0;
    int start_idx = 2;

    if (strcmp(argv[2], "--cached") == 0)
    {
        cached = 1;
        start_idx = 3;
    }

    if (start_idx >= argc)
    {
        return;
    }

    GegIndex *index = load_index();

    if (!index)
    {
        return;
    }

    FILE *index_file = fopen(".geg/index", "wb");

    if (!index_file)
    {
        perror("fopen");
        free_index(index);
        return;
    }

    size_t capacity = 4096;
    unsigned char *full_data = malloc(capacity);
    size_t offset = 0;

    unsigned char header[12];
    memcpy(header, "DIRC", 4);
    pack32_be(2, header + 4);
    pack32_be(0, header + 8);

    memcpy(full_data + offset, header, 12);
    offset += 12;

    int kept_count = 0;

    for (uint32_t i = 0; i < index->count; i++)
    {
        IndexEntry *ie = index->entries[i];
        int to_remove = 0;

        for (int j = start_idx; j < argc; j++)
        { 

            size_t arg_len = strlen(argv[j]);
            
            //If its the exact file
            if (strcmp(ie->path, argv[j]) == 0)
            {
                to_remove = 1;
                break;
            }

            //If its a folder
            else if(strncmp(ie->path,argv[j],arg_len)==0 && ie->path[arg_len]){
                to_remove = 1;
                break;
            }
        }

        if (to_remove)
        {
            if (!cached)
            {
                remove(ie->path);
            }
            continue;
        }

        unsigned char entry_buf[62];
        pack32_be(ie->ctime_sec, entry_buf);
        pack32_be(ie->ctime_nsec, entry_buf + 4);
        pack32_be(ie->mtime_sec, entry_buf + 8);
        pack32_be(ie->mtime_nsec, entry_buf + 12);
        pack32_be(ie->dev, entry_buf + 16);
        pack32_be(ie->ino, entry_buf + 20);
        pack32_be(ie->mode, entry_buf + 24);
        pack32_be(ie->uid, entry_buf + 28);
        pack32_be(ie->gid, entry_buf + 32);
        pack32_be(ie->size, entry_buf + 36);
        memcpy(entry_buf + 40, ie->sha1, 20);
        pack16_be(ie->flags, entry_buf + 60);

        unsigned int name_len = strlen(ie->path);

        if (offset + 62 + name_len + 8 + 20 > capacity)
        {
            capacity *= 2;
            full_data = realloc(full_data, capacity);
        }

        memcpy(full_data + offset, entry_buf, 62);
        offset += 62;

        memcpy(full_data + offset, ie->path, name_len);
        offset += name_len;

        int padding = 8 - ((62 + name_len) % 8);
        memset(full_data + offset, 0, padding);
        offset += padding;

        kept_count++;
    }

    pack32_be(kept_count, full_data + 8);

    unsigned char index_sha[20];
    sha1_hash(full_data, offset, index_sha);
    memcpy(full_data + offset, index_sha, 20);
    offset += 20;

    fwrite(full_data, 1, offset, index_file);
    fclose(index_file);
    free(full_data);
    free_index(index);
}
