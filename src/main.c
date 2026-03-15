#include "../include/core.h"
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
        geg_commit();
    }

    else if (strcmp(command, "cat") == 0)
    {
        geg_cat(argv[2]);
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

    else
    {
        printf("geg: '%s' is not a geg command.\n", command);
        return 1;
    }

    return 0;
}