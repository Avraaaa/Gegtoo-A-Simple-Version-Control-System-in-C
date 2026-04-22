#ifndef CORE_HEAD_TREE_H
#define CORE_HEAD_TREE_H

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
    char *path;
    unsigned char sha1[20];
} HeadEntry;

typedef struct {
    HeadEntry **entries;
    size_t count;
    size_t capacity;
} HeadTree;

void load_tree_entries(const char *tree_id, const char *base_path, HeadTree *head_tree);
HeadEntry *find_entry(HeadTree *tree, const char *path);
char **collect_paths(HeadTree *base, HeadTree *ours, HeadTree *theirs, int *count_out);

#endif
