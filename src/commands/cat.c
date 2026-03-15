#include "../../include/core.h"
#include "../../include/commands.h"

void geg_cat(const char *id)
{

    char obj_path[PATH_MAX];
    char temp_path[PATH_MAX];

    snprintf(obj_path, sizeof(obj_path), ".geg/objects/%.2s/%s", id, id + 2);
    snprintf(temp_path, sizeof(temp_path), ".geg/temp_read_%s", id);

    if (access(obj_path, F_OK) == -1)
    {

        printf("Object %s not found.\n", id);
        return;
    }

    decompress(obj_path, temp_path);

    FILE *fp = fopen(temp_path, "rb");

    if (!fp)
    {

        perror("fopen");
        return;
    }

    char type[10];
    size_t size;

    if (fscanf(fp, "%s %zu", type, &size) != 2)
    {

        printf("Invalid object header\n");
        fclose(fp);
        return;
    }

    fgetc(fp);

    if (strcmp(type, "tree") == 0)
    {

        long data_start = ftell(fp);

        while (ftell(fp) < data_start + size)
        {

            char mode[10];
            char name[255];
            unsigned char bin_sha[20];
            char hex_sha[41];

            if (fscanf(fp, "%s", mode) != 1)
                break;

            fgetc(fp);

            int i = 0;
            char c;

            while ((c = fgetc(fp)) != '\0' && c != EOF)
            {

                if (i < 254)
                    name[i++] = c;
            }

            name[i] = '\0';

            if (fread(bin_sha, 1, 20, fp) != 20)
                break;

            for (int i = 0; i < 20; i++)
            {

                sprintf(hex_sha + (i * 2), "%02x", bin_sha[i]);
            }
            printf("%s blob %s    %s\n", mode, hex_sha, name);
        }
    }

    else
    {

        char buffer[1024];
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0)
        {
            fwrite(buffer, 1, n, stdout);
        }
    }
    fclose(fp);
    remove(temp_path);
}
