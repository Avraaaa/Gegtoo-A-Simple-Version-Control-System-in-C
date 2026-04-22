#ifndef CORE_OBJECT_H
#define CORE_OBJECT_H

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
    char *data;
    size_t size;
    char type[10];
    char id[41];
} Blob;

#include "../../include/core/index.h"

void database_store(Blob *blob);
void restore_blob(const char *path, const char *id);
void restore_tree(const char *tree_id, const char *base_path);
char *geg_blob_content(const char *blob_id, size_t *size_out);
IndexEntry *store_and_index(const char *path, const char *content, size_t len);

#endif
