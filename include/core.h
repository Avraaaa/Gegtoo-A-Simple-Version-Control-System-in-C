#ifndef CORE_H
#define CORE_H

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
#include "utils/sha1.h"
#include "utils/huffman.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct
{
    char *data;
    size_t size;
    char type[10];
    char id[41];
} Blob;

typedef struct
{
    char *name;
    char *id;
} Entry;

typedef struct
{
    Entry **entries;
    size_t count;
} Tree;

typedef struct
{
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

typedef struct
{
    IndexEntry **entries;
    uint32_t count;
} GegIndex;

typedef struct
{
    char *path;
    unsigned char sha1[20];
} HeadEntry;

typedef struct
{
    HeadEntry **entries;
    size_t count;
    size_t capacity;
} HeadTree;

Entry *new_entry(char *name, char *id);
void free_entry(Entry *e);
int compare_Index_Entries_by_path(const void *a, const void *b);
int compare_entries_by_name(const void *a, const void *b);
void hex_to_binary(const char *hex, unsigned char *out);
unsigned char *serialize_tree(Tree *tree, size_t *out_size);
void pack32_be(uint32_t b, unsigned char *u);
void pack16_be(uint16_t b, unsigned char *u);
uint32_t unpack32_be(const unsigned char *b);
uint16_t unpack16_be(const unsigned char *b);
void create_directory(const char *path);
int is_ignored(const char *name);
char *read_workspace_files(const char *path, size_t *out_size);
void database_store(Blob *blob);
int get_head_ref_path(char *ref_path_out);
char *get_parent_commit_id();
void update_head_ref(const char *new_commit_id);
char **list_workspace_files(const char *root_path, int *count);
void restore_blob(const char *path, const char *id);
void restore_tree(const char *tree_id, const char *base_path);
GegIndex *load_index();
void free_index(GegIndex *index);
void get_commit_tree(const char *commit_id, char *tree_id_out);
void load_tree_entries(const char *tree_id, const char *base_path, HeadTree *head_tree);

#endif 
