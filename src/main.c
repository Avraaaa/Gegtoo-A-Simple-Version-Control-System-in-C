#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>

#ifndef PATH_MAX
    #define PATH_MAX 4096
#endif

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

void geg_init(const char *path)
{

    char buffer[PATH_MAX];
    char resolved[PATH_MAX];

    if (path == NULL)
    {
        strcpy(buffer, ".geg");
    }
    else
    {
        if (realpath(path, resolved) != NULL)
        {
            snprintf(buffer, sizeof(buffer), "%s/.geg", resolved);
        }
        else
        {
            if (path[0] == '/')
            {
                snprintf(buffer, sizeof(buffer), "%s/.geg", path);
            }
            else
            {
                char curr_work_dir[1024];

                if (getcwd(curr_work_dir, sizeof(curr_work_dir)) == NULL)
                {
                    perror("getcwd");
                    return;
                }
                snprintf(buffer, sizeof(buffer), "%s/%s/.geg", curr_work_dir, path);
            }
        }
    }

    char objects[PATH_MAX];
    char refs[PATH_MAX];
    char heads[PATH_MAX];

    snprintf(objects, sizeof(objects), "%s/objects", buffer);
    snprintf(refs, sizeof(refs), "%s/refs", buffer);
    snprintf(heads, sizeof(heads), "%s/refs/heads", buffer);

    create_directory(buffer);
    create_directory(objects);
    create_directory(refs);
    create_directory(heads);

    char head_path[PATH_MAX];
    snprintf(head_path, sizeof(head_path), "%s/HEAD", buffer);
    FILE *head_file = fopen(head_path, "w");

    if (head_file)
    {
        fprintf(head_file, "ref: refs/heads/master\n");
        fclose(head_file);
    }



    printf("Initialized an empty geg repository in %s/\n", buffer);
}

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        printf("Usage: ./geg <command> [<args>]\n");
        return 1;
    }

    const char *command = argv[1];

    if (strcmp(command, "init") == 0)
    {
        if (argc == 3)
        {
            geg_init(argv[2]);
        }
        else if (argc > 3)
        {
            printf("Too many arguments for init\n");
            printf("Usage: ./geg init [<directory>]\n");
            return 1;
        }
        else
        {
            geg_init(NULL);
        }
    }
    else
    {
        printf("geg: '%s' is not a geg command.\n", command);
        return 1;
    }
    return 0;
}