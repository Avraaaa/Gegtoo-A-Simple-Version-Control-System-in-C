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

int compare_Index_Entries_by_path(const void *a, const void *b)
{

    IndexEntry *ea = *(IndexEntry **)a;
    IndexEntry *eb = *(IndexEntry **)b;
    return strcmp(ea->path, eb->path);
}


GegIndex *load_index()
{
    FILE *fp = fopen(".geg/index", "rb");
    if (!fp)
        return NULL;

    unsigned char header[12];
    if (fread(header, 1, 12, fp) != 12)
    {
        fclose(fp);
        return NULL;
    }

    if (memcmp(header, "DIRC", 4) != 0 || unpack32_be(header + 4) != 2)
    {
        printf("Error: Invalid or unsupported index format.\n");
        fclose(fp);
        return NULL;
    }

    GegIndex *index = malloc(sizeof(GegIndex));
    index->count = unpack32_be(header + 8);
    index->entries = malloc(sizeof(IndexEntry *) * index->count);

    for (uint32_t i = 0; i < index->count; i++)
    {
        unsigned char fixed[62];
        fread(fixed, 1, 62, fp);

        IndexEntry *entry = malloc(sizeof(IndexEntry));
        entry->ctime_sec = unpack32_be(fixed);
        entry->ctime_nsec = unpack32_be(fixed + 4);
        entry->mtime_sec = unpack32_be(fixed + 8);
        entry->mtime_nsec = unpack32_be(fixed + 12);
        entry->dev = unpack32_be(fixed + 16);
        entry->ino = unpack32_be(fixed + 20);
        entry->mode = unpack32_be(fixed + 24);
        entry->uid = unpack32_be(fixed + 28);
        entry->gid = unpack32_be(fixed + 32);
        entry->size = unpack32_be(fixed + 36);
        memcpy(entry->sha1, fixed + 40, 20);
        entry->flags = unpack16_be(fixed + 60);

        int name_len = entry->flags & 0xFFF;
        entry->path = malloc(name_len + 1);
        fread(entry->path, 1, name_len, fp);
        entry->path[name_len] = '\0';

        int padding = 8 - ((62 + name_len) % 8);
        fseek(fp, padding, SEEK_CUR);

        index->entries[i] = entry;
    }

    fclose(fp);
    return index;
}


void free_index(GegIndex *index)
{
    if (!index)
        return;
    for (uint32_t i = 0; i < index->count; i++)
    {
        free(index->entries[i]->path);
        free(index->entries[i]);
    }
    free(index->entries);
    free(index);
}


