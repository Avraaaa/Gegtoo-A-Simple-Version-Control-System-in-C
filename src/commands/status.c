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
#include "../../include/core/tree.h"
#include "../../include/core/head_tree.h"
#include "../../include/core/object.h"
#include "../../include/utils/sha1.h"
#include "../../include/commands.h"
#include "../../include/core/refs.h"
#include "../../include/core/fs.h"

void geg_status(void)
{

    char root_path[PATH_MAX];

    if (getcwd(root_path, sizeof(root_path)) == NULL)
    {
        perror("getcwd");
        return;
    }

    int count = 0;
    char **files = list_workspace_files(root_path, &count);

    GegIndex *index = load_index();

    printf("Changes to be committed:\n");
    printf("  (use \"geg remove --staged <file>...\" to unstage)\n\n");

    int has_staged = 0;

    char *head_commit = get_parent_commit_id();
    HeadTree head_tree = {NULL, 0, 0};

    if (head_commit)
    {
        char tree_id[41] = {0};
        get_commit_tree(head_commit, tree_id);

        if (strlen(tree_id) > 0)
        load_tree_entries(tree_id, "", &head_tree);


        free(head_commit);
    }

    if (index)
    {
        for (uint32_t i = 0; i < index->count; i++)
        {
            IndexEntry *ie = index->entries[i];
            int found_in_head = 0;

            for (size_t j = 0; j < head_tree.count; j++)
            {
                if (strcmp(ie->path, head_tree.entries[j]->path) == 0)
                {
                    found_in_head = 1;

                    if (memcmp(ie->sha1, head_tree.entries[j]->sha1, 20) != 0)
                    {
                        printf("    modified:   %s\n", ie->path);
                        has_staged = 1;
                    }

                    break;
                }
            }

            if (!found_in_head)
            {
                printf("    new file:   %s\n", ie->path);
                has_staged = 1;
            }
        }
    }

    for (size_t j = 0; j < head_tree.count; j++)
    {
        int found_in_index = 0;

        if (index)
        {
            for (uint32_t i = 0; i < index->count; i++)
            {
                if (strcmp(head_tree.entries[j]->path, index->entries[i]->path) == 0)
                {
                    found_in_index = 1;
                    break;
                }
            }
        }

        if (!found_in_index)
        {
            printf("    deleted:    %s\n", head_tree.entries[j]->path);
            has_staged = 1;
        }

        free(head_tree.entries[j]->path);
        free(head_tree.entries[j]);
    }

    if (head_tree.entries)
        free(head_tree.entries);


    if (!has_staged)
        printf("    (none)\n");


    printf("\nChanges not staged for commit:\n");
    printf("  (use \"geg add <file>...\" to update what will be committed)\n");
    printf("  (use \"geg remove <file>...\" to discard changes in working directory)\n\n");

    int has_modified = 0;

    if (index)
    {
        for (uint32_t i = 0; i < index->count; i++)
        {
            IndexEntry *entry = index->entries[i];
            struct stat st;

            if (stat(entry->path, &st) == 0)
            {
                int modified = 0;

                if (entry->size != st.st_size)
        modified = 1;

                else if (entry->mtime_sec != (uint32_t)st.st_mtime)
                {
                    size_t size;
                    char *content = read_workspace_files(entry->path, &size);

                    if (content)
                    {
                        char header[64];
                        int header_len = snprintf(header, sizeof(header), "blob %zu", size);
                        size_t full_len = header_len + 1 + size;
                        unsigned char *full_data = malloc(full_len);

                        memcpy(full_data, header, header_len);
                        full_data[header_len] = '\0';
                        memcpy(full_data + header_len + 1, content, size);

                        unsigned char hash_bin[20];
                        sha1_hash(full_data, full_len, hash_bin);

                        free(full_data);
                        free(content);

                        if (memcmp(entry->sha1, hash_bin, 20) != 0)
        modified = 1;

                    }
                }

                if (modified)
                {
                    printf("    modified:   %s\n", entry->path);
                    has_modified = 1;
                }
            }
            else
            {
                printf("    deleted:    %s\n", entry->path);
                has_modified = 1;
            }
        }
    }

    if (!has_modified)
        printf("    (none)\n");


    printf("\nUntracked files:\n");
    printf("  (use \"geg add <file>...\" to include files to be committed)\n\n");

    int has_untracked = 0;

    for (int i = 0; i < count; i++)
    {
        int is_tracked = 0;

        if (index)
        {
            for (uint32_t j = 0; j < index->count; j++)
            {
                if (strcmp(files[i], index->entries[j]->path) == 0)
                {
                    is_tracked = 1;
                    break;
                }
            }
        }

        if (!is_tracked)
        {
            printf("    %s\n", files[i]);
            has_untracked = 1;
        }

        free(files[i]);
    }

    if (!has_untracked)
        printf("    (none)\n");


    if (files)
        free(files);


    if (index)
        free_index(index);

}
