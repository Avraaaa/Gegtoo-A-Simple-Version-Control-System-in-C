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


void write_index(IndexEntry **entries, int count) {
    FILE *index_file = fopen(".geg/index", "wb");
    if (!index_file) { perror("fopen"); return; }

    size_t capacity = 4096;
    unsigned char *full_data = malloc(capacity);
    size_t offset = 0;

    unsigned char header[12];
    memcpy(header, "DIRC", 4);
    pack32_be(2, header + 4);
    pack32_be(count, header + 8);
    memcpy(full_data + offset, header, 12);
    offset += 12;

    for (int i = 0; i < count; i++) {
        IndexEntry *e = entries[i];
        unsigned char entry_buf[62];
        pack32_be(e->ctime_sec,  entry_buf);
        pack32_be(e->ctime_nsec, entry_buf + 4);
        pack32_be(e->mtime_sec,  entry_buf + 8);
        pack32_be(e->mtime_nsec, entry_buf + 12);
        pack32_be(e->dev,        entry_buf + 16);
        pack32_be(e->ino,        entry_buf + 20);
        pack32_be(e->mode,       entry_buf + 24);
        pack32_be(e->uid,        entry_buf + 28);
        pack32_be(e->gid,        entry_buf + 32);
        pack32_be(e->size,       entry_buf + 36);
        memcpy(entry_buf + 40, e->sha1, 20);
        pack16_be(e->flags, entry_buf + 60);

        unsigned int name_len = strlen(e->path);
        if (offset + 62 + name_len + 8 + 20 > capacity) {
            capacity *= 2;
            full_data = realloc(full_data, capacity);
        }
        memcpy(full_data + offset, entry_buf, 62);  offset += 62;
        memcpy(full_data + offset, e->path, name_len); offset += name_len;

        int padding = 8 - ((62 + name_len) % 8);
        memset(full_data + offset, 0, padding);
        offset += padding;
    }

    unsigned char index_sha[20];
    sha1_hash(full_data, offset, index_sha);
    memcpy(full_data + offset, index_sha, 20);
    offset += 20;

    fwrite(full_data, 1, offset, index_file);
    fclose(index_file);
    free(full_data);
}