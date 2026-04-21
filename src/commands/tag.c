#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "../../include/core/refs.h"
#include "../../include/commands.h"

void geg_tag(int argc, char *argv[])
{

    // Checking if a geg repository has been initialized
    if (access(".geg", F_OK) == -1)
    {
        fprintf(stderr, "fatal: not a geg repository (or any of the parent directories): .geg\n");
        return;
    }
    //Listing tags: ./geg tag
    if (argc == 2)
    {
        DIR *dir = opendir(".geg/refs/tags");
        if (!dir)
            return;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            if (entry->d_name[0] == '.')
                continue;

            // Tags don't have an "active" state like branches, so we just print them all
            printf("%s\n", entry->d_name);
        }
        closedir(dir);
        return;
    }
    //Delete Tag: ./geg tag -d <tag_name>
    if (argc == 4 && strcmp(argv[2], "-d") == 0)
    {
        const char *target_tag = argv[3];

        if (!is_valid_ref_name(target_tag)) 
        {
            fprintf(stderr, "error: '%s' is not a valid tag name.\n", target_tag);
            return;
        }

        char tag_path[PATH_MAX];
        snprintf(tag_path, sizeof(tag_path), ".geg/refs/tags/%s", target_tag);

        if (unlink(tag_path) == 0)
        {
            printf("Deleted tag '%s'\n", target_tag);
        }
        else
        {
            fprintf(stderr, "error: tag '%s' not found.\n", target_tag);
        }
        return;
    }


    // Create Tag: ./geg tag <tag_name>
    if (argc == 3)
    {
        const char *new_tag = argv[2];

        if (!is_valid_ref_name(new_tag))
        {
            fprintf(stderr, "fatal: '%s' is not a valid tag name.\n", new_tag);
            return;
        }

        char *commit_id = get_parent_commit_id();
        if (!commit_id)
        {
            fprintf(stderr, "fatal: Failed to resolve HEAD as a valid ref.\n");
            return;
        }

        char tag_path[PATH_MAX];
        snprintf(tag_path, sizeof(tag_path), ".geg/refs/tags/%s", new_tag);

        // Checking if a tag already exists
        if (access(tag_path, F_OK) != -1)
        {
            fprintf(stderr, "fatal: tag '%s' already exists\n", new_tag);
            free(commit_id);
            return;
        }

        FILE *fp = fopen(tag_path, "w");
        if (fp)
        {
            fprintf(fp, "%s\n", commit_id);
            fclose(fp);
        }
        else
        {
            perror("Failed to create tag");
        }

        free(commit_id);
        return;
    }

    fprintf(stderr, "Usage: ./geg tag [<tagname> | -d <tagname>]\n");
}