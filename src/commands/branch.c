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

void geg_branch(int argc, char *argv[])
{
    // Checking if a geg repository has been initialized
    if (access(".geg", F_OK) == -1)
    {
        printf("fatal: not a geg repository (or any of the parent directories): .geg\n");
        return;
    }
    // Listing branches: ./geg branch
    if (argc == 2)
    {
        DIR *dir = opendir(".geg/refs/heads");
        if (!dir)
        {
            perror("Could not open refs directory");
            return;
        }
        char current_branch[256] = {0};
        get_current_branch(current_branch);

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            if (entry->d_name[0] == '.')
                continue;

            if (strcmp(entry->d_name, current_branch) == 0)
            {
                printf("* \033[32m%s\033[0m\n", entry->d_name); // Highlight current branch in green
            }
            else
            {
                printf("  %s\n", entry->d_name);
            }
        }
        closedir(dir);
        return;
    }

    // Delete Branch: ./geg branch -d <branch_name>

    if (argc == 4 && strcmp(argv[2], "-d") == 0)
    {
        const char *target_branch = argv[3];

        // Validating branch name before trying to delete
        if (!is_valid_ref_name(target_branch))
        {
            printf("error: '%s' is not a valid branch name.\n", target_branch);
            return;
        }

        char branch_path[PATH_MAX];
        snprintf(branch_path, sizeof(branch_path), ".geg/refs/heads/%s", target_branch);

        char current_branch[256] = {0};
        get_current_branch(current_branch);

        if (strcmp(current_branch, target_branch) == 0)
        {
            printf("error: Cannot delete branch '%s' checked out at HEAD\n", target_branch);
            return;
        }

        if (unlink(branch_path) == 0)
        printf("Deleted branch %s\n", target_branch);

        else
        {
            printf("error: branch '%s' not found.\n", target_branch);
        }
        return;
    }

    // Create branch: ./geg branch <branch_name>
    if (argc == 3)
    {
        const char *new_branch = argv[2];

        // 3b. Validate branch name before trying to create it
        if (!is_valid_ref_name(new_branch))
        {
            printf("fatal: '%s' is not a valid branch name.\n", new_branch);
            return;
        }

        char *commit_id = get_parent_commit_id();
        if (!commit_id)
        {
            printf("fatal: Not a valid object name: 'master'. (Do you need to make your first commit?)\n");
            return;
        }

        char branch_path[PATH_MAX];
        snprintf(branch_path, sizeof(branch_path), ".geg/refs/heads/%s", new_branch);

        // Check if branch already exists
        if (access(branch_path, F_OK) != -1)
        {
            printf("fatal: A branch named '%s' already exists.\n", new_branch);
            free(commit_id);
            return;
        }

        FILE *fp = fopen(branch_path, "w");
        if (fp)
        {
            fprintf(fp, "%s\n", commit_id);
            fclose(fp);
        }
        else
        {
            perror("Failed to create branch");
        }

        free(commit_id);
        return;
    }

    printf("Usage: ./geg branch [<branchname> | -d <branchname>]\n");
}