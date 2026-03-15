#include "../../include/core.h"
#include "../../include/commands.h"

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
