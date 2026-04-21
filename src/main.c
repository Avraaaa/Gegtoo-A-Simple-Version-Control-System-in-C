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

#include "../include/commands.h"

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
    else if (strcmp(command, "add") == 0)
    {
        geg_add(argc, argv);
    }
    else if (strcmp(command, "commit") == 0)
    {
        if (argc == 4 && strcmp(argv[2], "-m") == 0)
        {
            geg_commit(argv[3]);
        }
        else if (argc >= 3 && strcmp(argv[2], "-m") == 0)
        {
            printf("error:switch 'm' requires a value\n");
            printf("Usage: ./geg commit -m <message>\n");
        }

        else if (argc == 2)
        {
            geg_commit(NULL);
        }
        else
        {
            printf("Usage: ./geg commit [-m <message>]\n");
        }
    }
    else if (strcmp(command, "cat") == 0)
    {
        if (argc < 3)
        {
            printf("Usage: ./geg cat <object_id>\n");
        }
        else
        {
            geg_cat(argv[2]);
        }
    }
    else if (strcmp(command, "log") == 0)
    {
        geg_log();
    }
    else if (strcmp(command, "checkout") == 0)
    {
        if (argc < 3)
            printf("Usage: ./geg checkout <commit_id>\n");
        else
            geg_checkout(argv[2]);
    }
    else if (strcmp(command, "rm") == 0 || strcmp(command, "remove") == 0)
    {
        geg_remove(argc, argv);
    }
    else if (strcmp(command, "status") == 0)
    {
        geg_status();
    }
    else if (strcmp(command, "diff") == 0)
    {
        geg_diff(argc, argv);
    }
    else if (strcmp(command, "branch") == 0)
    {
        geg_branch(argc, argv);
    }
    else if (strcmp(command, "tag") == 0)
    {
        geg_tag(argc, argv);
    }
    else
    {
        printf("geg: '%s' is not a geg command.\n", command);
        return 1;
    }

    return 0;
}