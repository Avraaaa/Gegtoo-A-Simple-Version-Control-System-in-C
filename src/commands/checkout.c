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

#include "../../include/core/index.h"
#include "../../include/core/object.h"
#include "../../include/core/refs.h"
#include "../../include/utils/huffman.h"
#include "../../include/commands.h"

void geg_checkout(const char *target_id)
{

    char commit_id[41];
    int is_branch = 0;

    if (resolve_ref(target_id, commit_id) == 0)
    {
        is_branch = 1;
    }

    else
    {
        strncpy(commit_id, target_id, 40);
        commit_id[40] = '\0';
    }

    GegIndex *current_index = load_index();
    if (current_index)
    {
        for (uint32_t i = 0; i < current_index->count; i++)
        {
            if (strcmp(current_index->entries[i]->path, "geg") != 0 &&
                strcmp(current_index->entries[i]->path, "./geg") != 0)
            {
                remove(current_index->entries[i]->path);
            }
        }
        free_index(current_index);
    }
    remove(".geg/index");

    char obj_path[PATH_MAX];
    snprintf(obj_path, sizeof(obj_path), ".geg/objects/%.2s/%s", commit_id, commit_id + 2);

    if (access(obj_path, F_OK) == -1)
    {
        printf("Commit %s not found.\n", commit_id);
        return;
    }

    char temp_path[PATH_MAX];
    snprintf(temp_path, sizeof(temp_path), ".geg/temp_chk_%s", commit_id);
    decompress(obj_path, temp_path);

    FILE *fp = fopen(temp_path, "rb");
    if (!fp)
        return;

    char type[10];
    size_t size;
    fscanf(fp, "%s %zu", type, &size);
    fgetc(fp);

    char line[1024];
    char tree_id[41] = {0};
    if (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, "tree ", 5) == 0)
        {
            sscanf(line, "tree %40s", tree_id);
        }
    }

    fclose(fp);
    remove(temp_path);

    if (strlen(tree_id) == 0)
    {
        printf("Invalid commit object(no tree found).\n");
        return;
    }

    printf("Checking out commit %s (tree %s)...\n", commit_id, tree_id);
    restore_tree(tree_id, "");

    FILE *head = fopen(".geg/HEAD", "w");

    if (head)
    {
        if (is_branch && strcmp(target_id, "HEAD") != 0)
        {
            fprintf(head, "ref: refs/heads/%s\n", target_id);
            printf("Switched to branch '%s'\n", target_id);
        }
        else
        {
            fprintf(head, "%s\n", commit_id);
            printf("Note: checking out '%s'.\nYou are in 'detached HEAD' state.\n", commit_id);
        }
        fclose(head);
    }
}
