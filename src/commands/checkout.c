#include "../../include/core.h"
#include "../../include/commands.h"

void geg_checkout(const char *target_id)
{
    GegIndex *current_index = load_index();

    if(current_index){

        for(uint32_t i=0;i<current_index->count;i++){
            remove(current_index->entries[i]->path);
        }
        free_index(current_index);

    }
    remove(".geg/index");
    char obj_path[PATH_MAX];
    snprintf(obj_path, sizeof(obj_path), ".geg/objects/%.2s/%s", target_id, target_id + 2);

    if (access(obj_path, F_OK) == -1)
    {

        printf("Commit %s not found.\n", target_id);
        return;
    }

    char temp_path[PATH_MAX];
    snprintf(temp_path, sizeof(temp_path), ".geg/temp_chk_%s", target_id);
    decompress(obj_path, temp_path);

    FILE *fp = fopen(temp_path, "rb");
    if (!fp)
        return;

    char type[10];
    size_t size;
    fscanf(fp, "%s %zu", type, &size);
    fgetc(fp);

    char line[1024];
    char tree_id[41] = {0};
    if (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, "tree ", 5) == 0)
            sscanf(line, "tree %40s", tree_id);
    }

    fclose(fp);
    remove(temp_path);

    if (strlen(tree_id) == 0)
    {

        printf("Invalid commit object(no tree found).\n");
        return;
    }

    printf("Checking out commit %s (tree %s)...\n", target_id, tree_id);
    restore_tree(tree_id, "");

    FILE *head = fopen(".geg/HEAD", "w");

    if (head)
    {

        fprintf(head, "%s\n", target_id);
        fclose(head);
    }
}
