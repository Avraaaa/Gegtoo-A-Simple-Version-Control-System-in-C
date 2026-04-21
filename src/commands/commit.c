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
#include "../../include/commands.h"

void geg_commit(const char *message)
{
    if (message == NULL)
        message = "Automatic commit by geg";
    GegIndex *index = load_index();

    if (!index)
    {
        printf("nothing to commit\n");

        return;
    }

    Tree tree;
    tree.count = index->count;
    tree.entries = malloc(sizeof(Entry *) * tree.count);

    for (uint32_t i = 0; i < index->count; i++)
    {
        char hex_sha[41];

        for (int k = 0; k < 20; k++)
        {
            sprintf(hex_sha + (k * 2), "%02x", index->entries[i]->sha1[k]);
        }

        tree.entries[i] = new_entry(index->entries[i]->path, hex_sha);
    }

    size_t tree_size = 0;
    unsigned char *tree_data = serialize_tree(&tree, &tree_size);
    char tree_id[41] = {0};

    if (tree_data)
    {
        Blob tree_blob;
        tree_blob.data = (char *)tree_data;
        tree_blob.size = tree_size;
        strcpy(tree_blob.type, "tree");

        database_store(&tree_blob);
        strcpy(tree_id, tree_blob.id);

        free(tree_data);
    }

    for (size_t i = 0; i < tree.count; i++)
    {
        free_entry(tree.entries[i]);
    }

    free(tree.entries);
    free_index(index);

    char *parent_id = get_parent_commit_id();

    time_t now = time(NULL);
    struct tm local_tm = *localtime(&now);
    struct tm gmt_tm = *gmtime(&now);

    gmt_tm.tm_isdst = local_tm.tm_isdst;

    time_t local_time = mktime(&local_tm);
    time_t gmt_time = mktime(&gmt_tm);

    long offset = (long)difftime(local_time, gmt_time);

    char sign = '+';

    if (offset < 0)
    {
        sign = '-';
        offset = -offset;
    }

    int hours = offset / 3600;
    int mins = (offset % 3600) / 60;

    char time_str[32];
    sprintf(time_str, "%ld %c%02d%02d", (long)now, sign, hours, mins);

    const char *sys_user = getenv("USER");
    if (!sys_user)
    {
        sys_user = "Hornet";
    }

    char author[256];
    snprintf(author, sizeof(author), "%s <%s@local>", sys_user, sys_user);

    size_t commit_size = 1024 + (parent_id ? 50 : 0) + strlen(message);
    char *commit_content = malloc(commit_size);

    int content_offset = snprintf(commit_content, commit_size, "tree %s\n", tree_id);

    if (parent_id)
    {
        content_offset += snprintf(commit_content + content_offset, commit_size - content_offset, "parent %s\n", parent_id);
    }

    content_offset += snprintf(commit_content + content_offset, commit_size - content_offset,
                               "author %s %s\n"
                               "committer %s %s\n"
                               "\n"
                               "%s\n",
                               author, time_str, author, time_str, message);

    Blob commit_blob;
    commit_blob.data = commit_content;
    commit_blob.size = content_offset;
    strcpy(commit_blob.type, "commit");

    database_store(&commit_blob);

    printf("%s%s %s\n", commit_blob.id, parent_id ? "" : " (root-commit)", message);

    update_head_ref(commit_blob.id);

    free(commit_content);
    if (parent_id)
    {
        free(parent_id);
    }
}
