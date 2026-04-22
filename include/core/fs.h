#ifndef CORE_FS_H
#define CORE_FS_H

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

void create_directory(const char *path);
int write_file(const char *path, const char *content, size_t len);
void explore_directory(const char *base_path, const char *relative_path, char ***file_list, int *count);
int is_ignored(const char *name);
char *read_workspace_files(const char *path, size_t *out_size);
char **list_workspace_files(const char *root_path, int *count);

#endif
