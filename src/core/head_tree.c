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

#include "../../include/core/head_tree.h"
#include "../../include/core/binary.h"
#include "../../include/utils/huffman.h"

void load_tree_entries(const char *tree_id, const char *base_path, HeadTree *head_tree)
{

    char obj_path[PATH_MAX];
    char temp_path[PATH_MAX];
    snprintf(obj_path, sizeof(obj_path), ".geg/objects/%.2s/%s", tree_id, tree_id + 2);
    snprintf(temp_path, sizeof(temp_path), ".geg/temp_tree_read_%s", tree_id);

    if (access(obj_path, F_OK) == -1)
    {
        return;
    }

    decompress(obj_path, temp_path);
    FILE *fp = fopen(temp_path, "rb");

    if (!fp)
    {
        return;
    }

    char type[10];
    size_t size;

    if (fscanf(fp, "%s %zu", type, &size) != 2 || strcmp(type, "tree") != 0)
    {
        fclose(fp);
        return;
    }

    fgetc(fp);

    long data_start = ftell(fp);

    while (ftell(fp) < data_start + size)
    {
        char mode[10];
        char name[255];
        unsigned char bin_sha[20];
        char hex_sha[41];

        if (fscanf(fp, "%s", mode) != 1)
        {
            break;
        }

        fgetc(fp);

        int i = 0;
        char c;

        while ((c = fgetc(fp)) != '\0' && c != EOF && i < 254)
        {
            name[i++] = c;
        }

        name[i] = '\0';

        if (fread(bin_sha, 1, 20, fp) != 20)
        {
            break;
        }

        for (int k = 0; k < 20; k++)
        {
            sprintf(hex_sha + (k * 2), "%02x", bin_sha[k]);
        }

        char full_path[PATH_MAX];

        if (base_path && strlen(base_path) > 0)
        {
            snprintf(full_path, sizeof(full_path), "%s/%s", base_path, name);
        }
        else
        {
            snprintf(full_path, sizeof(full_path), "%s", name);
        }

        if (strcmp(mode, "100644") == 0)
        {
            if (head_tree->count >= head_tree->capacity)
            {
                head_tree->capacity = head_tree->capacity == 0 ? 10 : head_tree->capacity * 2;
                head_tree->entries = realloc(head_tree->entries, sizeof(HeadEntry *) * head_tree->capacity);
            }

            HeadEntry *he = malloc(sizeof(HeadEntry));
            he->path = strdup(full_path);
            memcpy(he->sha1, bin_sha, 20);
            head_tree->entries[head_tree->count++] = he;
        }
        else
        {
            load_tree_entries(hex_sha, full_path, head_tree);
        }
    }

    fclose(fp);
    remove(temp_path);
}

// Find specific file path within parsed tree
HeadEntry *find_entry(HeadTree *tree, const char *path)
{
    for (size_t i = 0; i < tree->count; i++)
        if (strcmp(tree->entries[i]->path, path) == 0)
            return tree->entries[i];
    return NULL;
}

// Collect a unique list of all file paths present across 'Base', 'Ours' and 'Theirs'
char **collect_paths(HeadTree *base, HeadTree *ours, HeadTree *theirs, int *count_out)
{
    int cap = 64, count = 0;
    char **paths = malloc(cap * sizeof(char *));

    HeadTree *trees[3] = {base, ours, theirs};
    for (int t = 0; t < 3; t++)
    {
        for (size_t i = 0; i < trees[t]->count; i++)
        {
            const char *p = trees[t]->entries[i]->path;
            int dup = 0;
            for (int j = 0; j < count; j++)
                if (strcmp(paths[j], p) == 0)
                {
                    dup = 1;
                    break;
                }
            if (dup)
                continue;

            if (count >= cap)
            {
                cap *= 2;
                paths = realloc(paths, cap * sizeof(char *));
            }
            paths[count++] = strdup(p);
        }
    }
    *count_out = count;
    return paths;
}
