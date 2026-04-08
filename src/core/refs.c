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

#include "../../include/core/refs.h"

int resolve_ref(const char *refname, char *hash_out)
{

    char path[PATH_MAX];

    // Default  is head
    if (strcmp(refname, "HEAD") == 0)
    {
        snprintf(path, sizeof(path), ".geg/HEAD");
    }

    // Leaves room for a different branch
    else
    {
        snprintf(path, sizeof(path), ".geg/refs/heads/%s", refname);
    }

    FILE *fp = fopen(path, "r");
    if (!fp)
    {
        return -1;
    }

    char buffer[1024];

    if (fscanf(fp, "%s", buffer) != 1)
    {
        fclose(fp);
        return -1;
    }

    // Attached
    if (strcmp(buffer, "ref:") == 0)
    {
        char ref_path[PATH_MAX];
        fscanf(fp, "%s", ref_path);
        fclose(fp);

        char full_ref_path[PATH_MAX];
        snprintf(full_ref_path, sizeof(full_ref_path), ".geg/%s", ref_path);

        FILE *ref_fp = fopen(full_ref_path, "r");

        if (!ref_fp)
        {
            return -1;
        }

        if (fscanf(ref_fp, "%40s", hash_out) != 1)
        {
            fclose(ref_fp);
            return -1;
        }

        fclose(ref_fp);
        return 0;
    }
    else
    {
        strncpy(hash_out, buffer, 41);
        fclose(fp);
        return 0;
    }
}


int get_head_ref_path(char *ref_path_out)
{

    char head_path[PATH_MAX];
    snprintf(head_path, sizeof(head_path), ".geg/HEAD");

    FILE *fp = fopen(head_path, "r");
    if (!fp)
        return -1;

    char buffer[1024];

    if (fscanf(fp, "ref: %s", buffer) != 1)
    {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    snprintf(ref_path_out, PATH_MAX, ".geg/%s", buffer);

    return 0;
}


char *get_parent_commit_id()
{

    char *commit_id = malloc(41);
    if (resolve_ref("HEAD", commit_id) == 0)
    {
        return commit_id;
    }

    free(commit_id);
    return NULL;
}


void update_head_ref(const char *new_commit_id)
{
    char head_path[PATH_MAX];
    snprintf(head_path, sizeof(head_path), ".geg/HEAD");

    FILE *fp = fopen(head_path, "r");
    if (!fp)
    {
        perror("Failed to read HEAD");
        return;
    }

    char buffer[1024];
    if (fscanf(fp, "%s", buffer) != 1)
    {
        fclose(fp);
        return;
    }

    if (strcmp(buffer, "ref:") == 0)
    {
        //Attached, update branch file
        char ref_path[PATH_MAX];
        fscanf(fp, "%s", ref_path);
        fclose(fp);

        char full_ref_path[PATH_MAX];
        snprintf(full_ref_path, sizeof(full_ref_path), ".geg/%s", ref_path);

        FILE *ref_fp = fopen(full_ref_path, "w");
        if (ref_fp)
        {
            fprintf(ref_fp, "%s\n", new_commit_id);
            fclose(ref_fp);
        }
        else
        {
            perror("Failed to update branch ref");
        }
    }
    else
    {
        // WDetached, overwrite raw hash
        fclose(fp); 

        FILE *head_out = fopen(head_path, "w");
        if (head_out)
        {
            fprintf(head_out, "%s\n", new_commit_id);
            fclose(head_out);
        }
        else
        {
            perror("Failed to update detached HEAD");
        }
    }
}


