#ifndef CORE_INDEX_H
#define CORE_INDEX_H

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

typedef struct {
    uint32_t ctime_sec;
    uint32_t ctime_nsec;
    uint32_t mtime_sec;
    uint32_t mtime_nsec;
    uint32_t dev;
    uint32_t ino;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    unsigned char sha1[20];
    uint16_t flags;
    char *path;
} IndexEntry;

typedef struct {
    IndexEntry **entries;
    uint32_t count;
} GegIndex;

int compare_Index_Entries_by_path(const void *a, const void *b);
GegIndex *load_index();
void free_index(GegIndex *index);

#endif
