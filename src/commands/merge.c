#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "../../include/commands.h"
#include "../../include/utils/lca.h"
#include "../../include/core/refs.h"
#include "../../include/core/object.h"
#include "../../include/core/tree.h"
#include "../../include/core/index.h"
#include "../../include/core/binary.h"
#include "../../include/core/fs.h"
#include "../../include/utils/sha1.h"

// Find specific file path within parsed tree
static HeadEntry *find_entry(HeadTree *tree, const char *path)
{
    for (size_t i = 0; i < tree->count; i++)
        if (strcmp(tree->entries[i]->path, path) == 0)
            return tree->entries[i];
    return NULL;
}

// Collect a unique list of all file paths present across 'Base', 'Ours' and 'Theirs'
static char **collect_paths(HeadTree *base, HeadTree *ours, HeadTree *theirs, int *count_out)
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

// Write string to working directory, creating missing folders
static int write_file(const char *path, const char *content, size_t len)
{
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash)
    {
        *slash = '\0';
        create_directory(dir);
    }

    FILE *fp = fopen(path, "wb");
    if (!fp)
        return -1;
    fwrite(content, 1, len, fp);
    fclose(fp);

    chmod(path, 0644); // Ensure files are readable/writable after creation
    return 0;
}

// Creates blob and writes to workspace, returning IndexEntry
static IndexEntry *store_and_index(const char *path, const char *content, size_t len)
{
    Blob b;
    b.data = (char *)content;
    b.size = len;
    strcpy(b.type, "blob");
    database_store(&b);

    write_file(path, content, len);

    struct stat st;
    if (stat(path, &st) != 0)
        return NULL;

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
    ie->size = (uint32_t)len;
    hex_to_binary(b.id, ie->sha1);
    ie->path = strdup(path);
    ie->flags = strlen(path) & 0xFFF;

    return ie;
}

// Serializes the current .geg/index into a tree object and returns its SHA1
static void build_and_store_tree(char tree_id_out[41])
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

void geg_merge(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Usage: ./geg merge <branch>\n");
        return;
    }
    const char *branch = argv[2];

    // resolve the commit ids
    char their_commit[41] = {0};
    if (resolve_ref(branch, their_commit) != 0)
    {
        strncpy(their_commit, branch, 40);
        their_commit[40] = '\0';
    }

    char our_commit[41] = {0};
    if (resolve_ref("HEAD", our_commit) != 0)
    {
        printf("error: no commits on HEAD yet\n");
        return;
    }

    if (strcmp(our_commit, their_commit) == 0)
    {
        printf("Already up to date.\n");
        return;
    }

    // Find Common Ancestor
    char base_commit[41] = {0};
    if (find_merge_base(our_commit, their_commit, base_commit) != 0)
    {
        printf("fatal: refusing to merge unrelated histories\n");
        return;
    }

    // Fast-Forward Merge Check
    if (strcmp(base_commit, our_commit) == 0)
    {
        printf("Fast-forward\n");
        char their_tree[41] = {0};
        get_commit_tree(their_commit, their_tree);
        restore_tree(their_tree, "");

        HeadTree ff = {NULL, 0, 0};
        load_tree_entries(their_tree, "", &ff);

        IndexEntry **ff_entries = malloc(sizeof(IndexEntry *) * ff.count);
        for (size_t i = 0; i < ff.count; i++)
        {
            HeadEntry *he = ff.entries[i];
            struct stat st;
            if (stat(he->path, &st) != 0)
            {
                ff_entries[i] = NULL;
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
            ff_entries[i] = ie;
            free(he->path);
            free(he);
        }
        free(ff.entries);

        qsort(ff_entries, ff.count, sizeof(IndexEntry *), compare_Index_Entries_by_path);
        write_index(ff_entries, (int)ff.count);

        for (size_t i = 0; i < ff.count; i++)
        {
            if (ff_entries[i])
            {
                free(ff_entries[i]->path);
                free(ff_entries[i]);
            }
        }
        free(ff_entries);

        update_head_ref(their_commit);
        printf("HEAD is now at %s\n", their_commit);
        return;
    }

    char base_tree[41] = {0}, our_tree[41] = {0}, their_tree[41] = {0};
    get_commit_tree(base_commit, base_tree);
    get_commit_tree(our_commit, our_tree);
    get_commit_tree(their_commit, their_tree);

    HeadTree base_files = {NULL, 0, 0};
    HeadTree our_files = {NULL, 0, 0};
    HeadTree their_files = {NULL, 0, 0};
    load_tree_entries(base_tree, "", &base_files);
    load_tree_entries(our_tree, "", &our_files);
    load_tree_entries(their_tree, "", &their_files);

    int path_count = 0;
    char **paths = collect_paths(&base_files, &our_files, &their_files, &path_count);

    IndexEntry **new_entries = malloc(sizeof(IndexEntry *) * path_count);
    int new_count = 0;
    int conflicts = 0;

    for (int pi = 0; pi < path_count; pi++)
    {
        const char *path = paths[pi];
        HeadEntry *be = find_entry(&base_files, path);
        HeadEntry *oe = find_entry(&our_files, path);
        HeadEntry *te = find_entry(&their_files, path);

        char base_sha[41] = {0}, our_sha[41] = {0}, their_sha[41] = {0};
        if (be)
            for (int k = 0; k < 20; k++)
                sprintf(base_sha + k * 2, "%02x", be->sha1[k]);
        if (oe)
            for (int k = 0; k < 20; k++)
                sprintf(our_sha + k * 2, "%02x", oe->sha1[k]);
        if (te)
            for (int k = 0; k < 20; k++)
                sprintf(their_sha + k * 2, "%02x", te->sha1[k]);

        // Added by us
        if (oe && !te && !be)
        {
            size_t sz = 0;
            char *content = get_blob_content(our_sha, &sz);
            if (content)
            {
                IndexEntry *ie = store_and_index(path, content, sz);
                if (ie)
                    new_entries[new_count++] = ie;
                free(content);
            }
            continue;
        }

        // Added by them
        if (te && !oe && !be)
        {
            size_t sz = 0;
            char *content = get_blob_content(their_sha, &sz);
            if (content)
            {
                IndexEntry *ie = store_and_index(path, content, sz);
                if (ie)
                    new_entries[new_count++] = ie;
                free(content);
            }
            continue;
        }

        // Added by both
        if (oe && te && !be)
        {
            if (memcmp(oe->sha1, te->sha1, 20) == 0)
            {
                size_t sz = 0;
                char *content = get_blob_content(our_sha, &sz);
                if (content)
                {
                    IndexEntry *ie = store_and_index(path, content, sz);
                    if (ie)
                    {
                        new_entries[new_count++] = ie;
                    }
                    free(content);
                }
            }
            else
            {
                size_t osz = 0, tsz = 0;
                char *oc = get_blob_content(our_sha, &osz);
                char *tc = get_blob_content(their_sha, &tsz);
                char *merged = NULL;
                size_t msz = 0;

                MergeStatus ms = three_way_merge("", 0, oc, osz, tc, tsz, &merged, &msz);
                if (ms != MERGE_CLEAN)
                {
                    printf("CONFLICT (add/add): %s\n", path);
                    conflicts++;
                }

                if (merged)
                {
                    IndexEntry *ie = store_and_index(path, merged, msz);
                    if (ie)
                        new_entries[new_count++] = ie;
                    free(merged);
                }
                free(oc);
                free(tc);
            }
            continue;
        }

        // Deleted by both
        if (!oe && !te && be)
        {
            remove(path);
            continue;
        }

        // Modified by us deleted by them
        if (!te && oe && be)
        {
            if (memcmp(oe->sha1, be->sha1, 20) == 0)
            {
                remove(path);
            }
            else
            {
                printf("CONFLICT (modify/delete): %s deleted in target, modified in HEAD\n", path);
                conflicts++;
                size_t sz = 0;
                char *content = get_blob_content(our_sha, &sz);
                if (content)
                {
                    IndexEntry *ie = store_and_index(path, content, sz);
                    if (ie)
                        new_entries[new_count++] = ie;
                    free(content);
                }
            }
            continue;
        }

        // Modified by then deleted by us
        if (!oe && te && be)
        {
            if (memcmp(te->sha1, be->sha1, 20) == 0)
            {
                remove(path);
            }
            else
            {
                printf("CONFLICT (delete/modify): %s deleted in HEAD, modified in target\n", path);
                conflicts++;
                size_t sz = 0;
                char *content = get_blob_content(their_sha, &sz);
                if (content)
                {
                    IndexEntry *ie = store_and_index(path, content, sz);
                    if (ie)
                        new_entries[new_count++] = ie;
                    free(content);
                }
            }
            continue;
        }

        // Modified in both (True 3-way file merge)
        if (oe && te && be)
        {
            if (memcmp(oe->sha1, te->sha1, 20) == 0)
            {
                size_t sz = 0;
                char *content = get_blob_content(our_sha, &sz);
                if (content)
                {
                    IndexEntry *ie = store_and_index(path, content, sz);
                    if (ie)
                        new_entries[new_count++] = ie;
                    free(content);
                }
                continue;
            }

            size_t bsz = 0, osz = 0, tsz = 0;
            char *bc = get_blob_content(base_sha, &bsz);
            char *oc = get_blob_content(our_sha, &osz);
            char *tc = get_blob_content(their_sha, &tsz);

            char *merged = NULL;
            size_t msz = 0;
            MergeStatus ms = three_way_merge(
                bc ? bc : "", bsz,
                oc ? oc : "", osz,
                tc ? tc : "", tsz,
                &merged, &msz);

            if (ms != MERGE_CLEAN)
            {
                printf("CONFLICT (content): Merge conflict in %s\n", path);
                conflicts++;
            }

            if (merged)
            {
                IndexEntry *ie = store_and_index(path, merged, msz);
                if (ie)
                    new_entries[new_count++] = ie;
                free(merged);
            }
            free(bc);
            free(oc);
            free(tc);
        }
    }

    // Free extracted trees and paths
    for (size_t i = 0; i < base_files.count; i++)
    {
        free(base_files.entries[i]->path);
        free(base_files.entries[i]);
    }
    for (size_t i = 0; i < our_files.count; i++)
    {
        free(our_files.entries[i]->path);
        free(our_files.entries[i]);
    }
    for (size_t i = 0; i < their_files.count; i++)
    {
        free(their_files.entries[i]->path);
        free(their_files.entries[i]);
    }
    free(base_files.entries);
    free(our_files.entries);
    free(their_files.entries);
    for (int i = 0; i < path_count; i++)
        free(paths[i]);
    free(paths);

    // Update Index
    qsort(new_entries, new_count, sizeof(IndexEntry *), compare_Index_Entries_by_path);
    write_index(new_entries, new_count);

    for (int i = 0; i < new_count; i++)
    {
        free(new_entries[i]->path);
        free(new_entries[i]);
    }
    free(new_entries);

    if (conflicts > 0)
    {
        printf("Automatic merge failed; fix conflicts and then commit the result.\n");
        return;
    }

    // Generate 2-parent Merge Commit
    char merge_tree[41] = {0};
    build_and_store_tree(merge_tree);

    time_t now = time(NULL);
    struct tm local_tm = *localtime(&now);
    struct tm gmt_tm   = *gmtime(&now);
    gmt_tm.tm_isdst = local_tm.tm_isdst;
    long offset = (long)difftime(mktime(&local_tm), mktime(&gmt_tm));
    char sign = (offset < 0) ? (offset = -offset, '-') : '+';
    char time_str[32];
    sprintf(time_str, "%ld %c%02d%02d", (long)now, sign, (int)(offset/3600), (int)((offset%3600)/60));
    
    const char *sys_user = getenv("USER");
    if (!sys_user) sys_user = "Hornet";
    char author[256];
    snprintf(author, sizeof(author), "%s <%s@local>", sys_user, sys_user);

    char msg[256];
    snprintf(msg, sizeof(msg), "Merge branch '%s'", branch);
    size_t commit_cap = 1024;
    char *commit_content = malloc(commit_cap);
    int coff = snprintf(commit_content, commit_cap,
        "tree %s\n"
        "parent %s\n"
        "parent %s\n"
        "author %s %s\n"
        "committer %s %s\n"
        "\n"
        "%s\n",
        merge_tree,
        our_commit, their_commit,
        author, time_str,
        author, time_str,
        msg);
    Blob cb;
    cb.data = commit_content;
    cb.size = coff;
    strcpy(cb.type, "commit");
    database_store(&cb);

    printf("Merge made by the 'geg' strategy.\n");
    printf("[%s] %s\n",cb.id,msg);

    update_head_ref(cb.id);
    free(commit_content);
}