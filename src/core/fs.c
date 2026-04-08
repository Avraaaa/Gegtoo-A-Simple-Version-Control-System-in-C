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

#include "../../include/core/fs.h"

void create_directory(const char *path)
{

    char tmp[PATH_MAX];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);

    if (tmp[len - 1] == '/')
    {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = 0;

            if (mkdir(tmp, 0755) != 0)
            {
                if (errno != EEXIST)
                {
                    perror("mkdir");
                }
            };
            *p = '/';
        }
    }

    if (mkdir(tmp, 0755) != 0)
    {
        if (errno != EEXIST)
        {
            perror("mkdir");
        }
    };
}


int is_ignored(const char *name)
{

    struct stat path_stat;

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0 || strcmp(name, ".geg") == 0 || strcmp(name, ".git") == 0)
    {
        return 1;
    }

    // if (stat(name, &path_stat) == 0 && S_ISDIR(path_stat.st_mode))
    // {
    //     return 1;
    // }
    return 0;
}


char *read_workspace_files(const char *path, size_t *out_size)
{

    FILE *fp = fopen(path, "rb");

    if (!fp)
    {
        perror("fopen");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    size_t length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buffer = malloc(length);

    if (!buffer)
    {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(fp);
        return NULL;
    }

    fread(buffer, 1, length, fp);
    fclose(fp);

    *out_size = length;
    return buffer;
}


void explore_directory(const char *base_path, const char *relative_path, char ***file_list, int *count)
{

    char current_dir_path[PATH_MAX];
    // use base path if root, else append relative path
    if (strlen(relative_path) == 0)
    {
        snprintf(current_dir_path, sizeof(current_dir_path), "%s", base_path);
    }
    else
    {
        snprintf(current_dir_path, sizeof(current_dir_path), "%s/%s", base_path, relative_path);
    }

    DIR *dir = opendir(current_dir_path);
    if (!dir)
        return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {

        if (is_ignored(entry->d_name))
            continue;

        char new_relative_path[PATH_MAX];
        // build new relative path
        if (strlen(relative_path) == 0)
        {
            snprintf(new_relative_path, sizeof(new_relative_path), "%s", entry->d_name);
        }
        else
        {
            snprintf(new_relative_path, sizeof(new_relative_path), "%s/%s", relative_path, entry->d_name);
        }

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, new_relative_path);

        struct stat st;
        if (stat(full_path, &st) == 0)
        {

            if (S_ISDIR(st.st_mode))
            {
                // If the path is a folder, recursively call it again to explore subdirectories and files
                explore_directory(base_path, new_relative_path, file_list, count);
            }

            else if (S_ISREG(st.st_mode))
            {
                char **temp = realloc(*file_list, sizeof(char *) * (*count + 1));
                if (temp)
                {
                    *file_list = temp;
                    (*file_list)[*count] = strdup(new_relative_path);
                    (*count)++;
                }
            }
        }
    }
    closedir(dir);
}


char **list_workspace_files(const char *root_path, int *count)
{
    char **file_list = NULL;
    *count = 0;
    explore_directory(root_path, "", &file_list, count);
    return file_list;
}


