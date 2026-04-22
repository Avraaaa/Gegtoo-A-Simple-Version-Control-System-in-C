#ifndef CORE_REFS_H
#define CORE_REFS_H

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

int resolve_ref(const char *refname, char *hash_out);

char *get_parent_commit_id();
void update_head_ref(const char *new_commit_id);
void get_current_branch(char *branch_out);
int is_valid_ref_name(const char *name);


#endif
