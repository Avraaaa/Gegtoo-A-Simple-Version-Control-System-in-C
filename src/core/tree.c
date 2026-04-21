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


void get_commit_tree(const char *commit_id, char *tree_id_out)
{

    char obj_path[PATH_MAX];
    char temp_path[PATH_MAX];
    snprintf(obj_path, sizeof(obj_path), ".geg/objects/%.2s/%s", commit_id, commit_id + 2);
    snprintf(temp_path, sizeof(temp_path), ".geg/temp_read_%s", commit_id);

    if (access(obj_path, F_OK) == -1)
    {
        tree_id_out[0] = '\0';
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
    fscanf(fp, "%s %zu", type, &size);
    fgetc(fp);

    char line[1024];

    if (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, "tree ", 5) == 0)
        {
            sscanf(line, "tree %40s", tree_id_out);
        }
    }

    fclose(fp);
    remove(temp_path);
}


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


void read_commit_parents(const char *commit_id, char parents[2][41], int *count)
{
    char obj_path[PATH_MAX];
    char temp_path[PATH_MAX];
    snprintf(obj_path,  sizeof(obj_path),  ".geg/objects/%.2s/%s", commit_id, commit_id + 2);
    snprintf(temp_path, sizeof(temp_path), ".geg/temp_par_%s", commit_id);

    *count = 0;
    
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
    
    if (fscanf(fp, "%s %zu", type, &size) != 2 || strcmp(type, "commit") != 0)
    {
        fclose(fp);
        remove(temp_path);
        return;
    }
    
    fgetc(fp);

    char line[1024];
    
    while (fgets(line, sizeof(line), fp))
    {
        if (line[0] == '\n')
        {
            break;
        }
        if (strncmp(line, "parent ", 7) == 0 && *count < 2)
        {
            sscanf(line, "parent %40s", parents[(*count)++]);
        }
    }
    
    fclose(fp);
    remove(temp_path);
}