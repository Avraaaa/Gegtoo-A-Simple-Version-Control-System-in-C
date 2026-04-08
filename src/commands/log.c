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
#include "../../include/utils/huffman.h"
#include "../../include/commands.h"

void geg_log(void)
{

    char current_id[41];

    if (resolve_ref("HEAD", current_id) != 0)
    {
        printf("HEAD not found or no commits yet. :(\n");
        return;
    }

    while (1)
    {
        char obj_path[PATH_MAX];
        char temp_path[PATH_MAX];

        snprintf(obj_path, sizeof(obj_path), ".geg/objects/%.2s/%s", current_id, current_id + 2);
        snprintf(temp_path, sizeof(temp_path), ".geg/temp_log_%s", current_id);

        if (access(obj_path, F_OK) == -1)
        {
            printf("Error: Object %s is missing.\n", current_id);
            break;
        }

        decompress(obj_path, temp_path);

        FILE *fp = fopen(temp_path, "rb");

        if (!fp)
        {
            perror("fopen");
            break;
        }

        char type[10];
        size_t size;
        if (fscanf(fp, "%s %zu", type, &size) != 2 || strcmp(type, "commit"))
        {
            printf("Error: Object %s is not a commit.\n", current_id);
            fclose(fp);
            remove(temp_path);
            break;
        }

        fgetc(fp);

        char line[1024];
        char parent_id[41] = {0};
        char author[255] = {0};

        while (fgets(line, sizeof(line), fp))
        {

            if (line[0] == '\n')
                break;

            if (strncmp(line, "parent ", 7) == 0)
            {
                sscanf(line, "parent %40s", parent_id);
            }

            else if (strncmp(line, "author ", 7) == 0)
            {
                char *p = line + 7;
                size_t len = strlen(p);
                if (p[len - 1] == '\n')
                    p[len - 1] = 0;
                strcpy(author, p);
            }
        }

        printf("commit %s\n", current_id);
        printf("Author: %s\n", author);

        printf("\n");

        while (fgets(line, sizeof(line), fp))
        {
            printf("    %s", line);
        }
        printf("\n");

        fclose(fp);

        remove(temp_path);

        if (strlen(parent_id) > 0)
        {
            strcpy(current_id, parent_id);
        }

        else
        {
            break;
        }
    }
}
