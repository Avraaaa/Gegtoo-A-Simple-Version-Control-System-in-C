#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include "utils/sha1.h"
#include "utils/huffman.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct
{
    char *data;
    size_t size;
    char type[10];
    char id[41];
} Blob;

typedef struct
{
    char *name;
    char *id;
} Entry;

typedef struct
{
    Entry **entries;
    size_t count;
} Tree;

Entry *new_entry(char *name, char *id)
{

    Entry *e = malloc(sizeof(Entry));
    e->name = strdup(name);
    e->id = strdup(id);
    return e;
}

void free_entry(Entry *e)
{

    if (e)
    {
        free(e->name);
        free(e->id);
        free(e);
    }
}

int compare_entries(const void *a, const void *b)
{
    Entry *ea = *(Entry **)a;
    Entry *eb = *(Entry **)b;
    return strcmp(ea->name, eb->name);
}

void hex_to_binary(const char *hex, unsigned char *out)
{

    for (int i = 0; i < 20; i++)
    {
        sscanf(hex + 2 * i, "%2hhx", &out[i]);
    }
}

unsigned char *serialize_tree(Tree *tree, size_t *out_size)
{

    qsort(tree->entries, tree->count, sizeof(Entry *), compare_entries);

    size_t total_size = 0;
    const char *MODE = "100644";

    for (size_t i = 0; i < tree->count; i++)
    {
        total_size += strlen(MODE) + 1 + strlen(tree->entries[i]->name) + 1 + 20;
    }

    unsigned char *buffer = malloc(total_size);
    size_t offset = 0;

    for (size_t i = 0; i < tree->count; i++)
    {

        Entry *entry = tree->entries[i];

        int written = sprintf((char *)(buffer + offset), "%s %s", MODE, entry->name);
        offset += written;

        buffer[offset++] = '\0';

        hex_to_binary(entry->id, buffer + offset);
        offset += 20;
    }

    *out_size = total_size;
    return buffer;
}

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

int is_ignored(const char *name)
{

    struct stat path_stat;

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0 || strcmp(name, ".geg") == 0 || strcmp(name, ".git") == 0)
    {
        return 1;
    }

    if (stat(name, &path_stat) == 0 && S_ISDIR(path_stat.st_mode))
    {
        return 1;
    }

    else
    {
        return 0;
    }
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

void database_store(Blob *blob)
{
    char header[64];
    int header_len = snprintf(header, sizeof(header), "%s %zu", blob->type, blob->size);

    size_t full_len = header_len + 1 + blob->size;
    unsigned char *full_data = malloc(full_len);
    if (!full_data)
    {
        perror("Malloc failed");
        return;
    }

    memcpy(full_data, header, header_len);
    full_data[header_len] = '\0';
    memcpy(full_data + header_len + 1, blob->data, blob->size);

    unsigned char hash_bin[20];
    sha1_hash(full_data, full_len, hash_bin);

    for (int i = 0; i < 20; i++)
    {
        sprintf(blob->id + (i * 2), "%02x", hash_bin[i]);
    }

    char dir_path[PATH_MAX];
    char final_path[PATH_MAX];
    char temp_path[PATH_MAX];

    snprintf(dir_path, sizeof(dir_path), ".geg/objects/%.2s", blob->id);
    snprintf(final_path, sizeof(final_path), "%s/%s", dir_path, blob->id + 2);
    snprintf(temp_path, sizeof(temp_path), ".geg/temp_%s", blob->id);

    FILE *tmp = fopen(temp_path, "wb");
    if (!tmp)
    {
        perror("Failed to create temp file");
        free(full_data);
        return;
    }

    fwrite(full_data, 1, full_len, tmp);
    fclose(tmp);

    create_directory(dir_path);
    compress(temp_path, final_path);
    remove(temp_path);
    free(full_data);
}

char **list_workspace_files(const char *root_path, int *count)
{

    DIR *dir;
    struct dirent *entry;
    char **file_list = NULL;

    dir = opendir(root_path);

    if (dir == NULL)
    {
        perror("opendir");
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (is_ignored(entry->d_name))
        {
            continue;
        }

        char **temp = realloc(file_list, sizeof(char *) * (*count + 1));

        if (!temp)
        {
            perror("realloc");
            break;
        }

        file_list = temp;
        file_list[*count] = strdup(entry->d_name);
        (*count)++;
    }

    closedir(dir);
    return file_list;
}

void geg_commit(void)
{

    char root_path[PATH_MAX];

    if (getcwd(root_path, sizeof(root_path)) == NULL)
    {
        perror("getcwd");
        return;
    }

    int count = 0;
    char **files = list_workspace_files(root_path, &count);

    if (files)
    {

        Tree tree;
        tree.count = count;
        tree.entries = malloc(sizeof(Entry *) * count);

        if (!tree.entries)
        {

            perror("malloc");
            return;
        }

        int valid_count = 0;

        for (int i = 0; i < count; i++)
        {

            size_t size;
            char *content = read_workspace_files(files[i], &size);

            if (content)
            {

                Blob blob;
                blob.data = content;
                blob.size = size;
                strcpy(blob.type, "blob");
                database_store(&blob);

                tree.entries[valid_count] = new_entry(files[i], blob.id);
                valid_count++;

                free(content);
            }
            free(files[i]);
        }
        free(files);

        tree.count = valid_count;
        size_t tree_size = 0;
        unsigned char *tree_data = serialize_tree(&tree, &tree_size);

        if (tree_data)
        {

            Blob tree_blob;
            tree_blob.data = (char *)tree_data;
            tree_blob.size = tree_size;
            strcpy(tree_blob.type, "tree");

            database_store(&tree_blob);
            printf("tree %s\n", tree_blob.id);

            free(tree_data);
        }

        for (size_t i = 0; i < tree.count; i++)
        {

            free_entry(tree.entries[i]);
        }
        free(tree.entries);
    }
}

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
            printf("%s blob %s\t%s\n", mode, hex_sha, name);
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

    else if (strcmp(command, "commit") == 0)
    {
        geg_commit();
    }

    else if (strcmp(command, "cat") == 0)
    {
        geg_cat(argv[2]);
    }

    else
    {
        printf("geg: '%s' is not a geg command.\n", command);
        return 1;
    }
    return 0;
}