#ifndef UTILS_MYERS_DIFF_H
#define UTILS_MYERS_DIFF_H

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
    int prev_x, prev_y, x, y;
} EditItem;

void diff_file(const char* filepath, const unsigned char* old_sha1);

#endif
