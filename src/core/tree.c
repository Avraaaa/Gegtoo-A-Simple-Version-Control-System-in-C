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
#include "../../include/core/tree.h"
#include "../../include/core/index.h"
#include "../../include/core/object.h"
#include "../../include/utils/huffman.h"

Entry *new_entry(char *name, char *id)
{

    Entry *e = malloc(sizeof(Entry));
    e->name = strdup(name);
    e->id = strdup(id);
    return e;
}


void free_entry(Entry *e)
{

    if (e)
    {
        free(e->name);
        free(e->id);
        free(e);
    }
}


int compare_entries_by_name(const void *a, const void *b)
{
    Entry *ea = *(Entry **)a;
    Entry *eb = *(Entry **)b;
    return strcmp(ea->name, eb->name);
}


unsigned char *serialize_tree(Tree *tree, size_t *out_size)
{

    qsort(tree->entries, tree->count, sizeof(Entry *), compare_entries_by_name);

    size_t total_size = 0;
    const char *MODE = "100644";

    for (size_t i = 0; i < tree->count; i++)
    {
        total_size += strlen(MODE) + 1 + strlen(tree->entries[i]->name) + 1 + 20;
    }

    unsigned char *buffer = malloc(total_size);
    size_t offset = 0;

    for (size_t i = 0; i < tree->count; i++)
    {

        Entry *entry = tree->entries[i];

        int written = sprintf((char *)(buffer + offset), "%s %s", MODE, entry->name);
        offset += written;

        buffer[offset++] = '\0';

        hex_to_binary(entry->id, buffer + offset);
        offset += 20;
    }

    *out_size = total_size;
    return buffer;
}


// Serializes the current .geg/index into a tree object and returns its SHA1
void build_and_store_tree(char tree_id_out[41])
{
    GegIndex *idx = load_index();
    if (!idx)
    {
        tree_id_out[0] = '\0';
        return;
    }

    Tree tree;
    tree.count = idx->count;
    tree.entries = malloc(sizeof(Entry *) * tree.count);
    for (uint32_t i = 0; i < idx->count; i++)
    {
        char hex[41];
        for (int k = 0; k < 20; k++)
            sprintf(hex + k * 2, "%02x", idx->entries[i]->sha1[k]);
        tree.entries[i] = new_entry(idx->entries[i]->path, hex);
    }

    size_t tree_sz = 0;
    unsigned char *tree_data = serialize_tree(&tree, &tree_sz);

    if (tree_data)
    {
        Blob tb;
        tb.data = (char *)tree_data;
        tb.size = tree_sz;
        strcpy(tb.type, "tree");
        database_store(&tb);
        strcpy(tree_id_out, tb.id);
        free(tree_data);
    }
    else
    {
        tree_id_out[0] = '\0';
    }

    for (size_t i = 0; i < tree.count; i++)
        free_entry(tree.entries[i]);
    free(tree.entries);
    free_index(idx);
}