#ifndef CORE_TREE_H
#define CORE_TREE_H

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
    char *name;
    char *id;
} Entry;

typedef struct {
    Entry **entries;
    size_t count;
} Tree;

Entry *new_entry(char *name, char *id);
void free_entry(Entry *e);
int compare_entries_by_name(const void *a, const void *b);
unsigned char *serialize_tree(Tree *tree, size_t *out_size);
void build_and_store_tree(char tree_id_out[41]);

#endif