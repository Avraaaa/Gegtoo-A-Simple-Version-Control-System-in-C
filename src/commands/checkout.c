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
#include "../../include/core/tree.h"
#include "../../include/core/head_tree.h"
#include "../../include/utils/huffman.h"
#include "../../include/commands.h"

void geg_checkout(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Usage: ./geg checkout [-b] <target>\n");
        return;
    }

    const char *target_id = argv[2];

    // Handle the -b flag for inline branch creation
    if (strcmp(argv[2], "-b") == 0)
    {
        if (argc < 4)
        {
            printf("error: switch 'b' requires a value\n");
            return;
        }
        
        target_id = argv[3];

        if (!is_valid_ref_name(target_id))
        {
            printf("fatal: '%s' is not a valid branch name.\n", target_id);
            return;
        }

        char *current_commit = get_parent_commit_id();
        if (!current_commit)
        {
            printf("fatal: Not a valid object name: 'master'. (Do you need to make your first commit?)\n");
            return;
        }

        char branch_path[PATH_MAX];
        snprintf(branch_path, sizeof(branch_path), ".geg/refs/heads/%s", target_id);

        if (access(branch_path, F_OK) != -1)
        {
            printf("fatal: A branch named '%s' already exists.\n", target_id);
            free(current_commit);
            return;
        }

        FILE *fp = fopen(branch_path, "w");
        if (fp)
        {
            fprintf(fp, "%s\n", current_commit);
            fclose(fp);
        }
        else
        {
            perror("Failed to create branch");
            free(current_commit);
            return;
        }
        free(current_commit);
    }

    char commit_id[41];
    int is_branch = 0;

    // Resolving target to a commit hash and check if it's a branch
    if (resolve_ref(target_id, commit_id) == 0)
    {
        char branch_path[PATH_MAX];
        snprintf(branch_path, sizeof(branch_path), ".geg/refs/heads/%s", target_id);
        
        if (access(branch_path, F_OK) == 0) 
        {
            is_branch = 1;
        }
    }
    else
    {
        strncpy(commit_id, target_id, 40);
        commit_id[40] = '\0';
    }

    GegIndex *current_index = load_index();

    if (current_index)
    {
        // Removing existing tracked files, protecting the geg binary
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

    HeadTree ht = {NULL, 0, 0};
    load_tree_entries(tree_id, "", &ht);

    IndexEntry **entries = malloc(sizeof(IndexEntry *) * ht.count);
    int entry_count = 0;

    for (size_t i = 0; i < ht.count; i++)
    {
        HeadEntry *he = ht.entries[i];
        struct stat st;
        if (stat(he->path, &st) != 0)
        {
            free(he->path);
            free(he);
            continue;
        }
        IndexEntry *ie = malloc(sizeof(IndexEntry));
        ie->ctime_sec = (uint32_t)st.st_ctime;
        ie->ctime_nsec = 0;
        ie->mtime_sec = (uint32_t)st.st_mtime;
        ie->mtime_nsec = 0;
        ie->dev = st.st_dev;
        ie->ino = st.st_ino;
        ie->mode = st.st_mode;
        ie->uid = st.st_uid;
        ie->gid = st.st_gid;
        ie->size = (uint32_t)st.st_size;
        memcpy(ie->sha1, he->sha1, 20);
        ie->path = strdup(he->path);
        ie->flags = strlen(he->path) & 0xFFF;
        entries[entry_count++] = ie;
        free(he->path);
        free(he);
    }
    free(ht.entries);

    qsort(entries, entry_count, sizeof(IndexEntry *), compare_Index_Entries_by_path);
    write_index(entries, entry_count);

    for (int i = 0; i < entry_count; i++)
    {
        free(entries[i]->path);
        free(entries[i]);
    }
    free(entries);

    FILE *head = fopen(".geg/HEAD", "w");

    if (head)
    {
        // Update HEAD: use symbolic ref for branches, raw hash for tags/commits
        if (is_branch && strcmp(target_id, "HEAD") != 0)
        {
            fprintf(head, "ref: refs/heads/%s\n", target_id);
            printf("Switched to branch '%s'\n", target_id);
        }
        else{
            fprintf(head, "%s\n", commit_id);
            printf("Note: checking out '%s'.\nYou are in 'detached HEAD' state.\n", commit_id);
        }
        fclose(head);
    }
}