#include "../../include/core.h"

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

int compare_Index_Entries_by_path(const void *a, const void *b){

    IndexEntry *ea = *(Entry**)a;
    IndexEntry *eb = *(Entry**)b;
    return strcmp(ea->path,eb->path);
}

int compare_entries_by_name(const void *a, const void *b)
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

    qsort(tree->entries, tree->count, sizeof(Entry *), compare_entries_by_name);

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

void pack32_be(uint32_t b, unsigned char *u)
{

    u[0] = (b >> 24) & 0xFF;
    u[1] = (b >> 16) & 0xFF;
    u[2] = (b >> 8) & 0xFF;
    u[3] = (b) & 0xFF;
}

void pack16_be(uint16_t b, unsigned char *u)
{

    u[0] = (b >> 8) & 0xFF;
    u[1] = (b) & 0xFF;
}

uint32_t unpack32_be(const unsigned char *b)
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

uint16_t unpack16_be(const unsigned char *b)
{
    return ((uint16_t)b[0] << 8) | b[1];
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

int get_head_ref_path(char *ref_path_out)
{

    char head_path[PATH_MAX];
    snprintf(head_path, sizeof(head_path), ".geg/HEAD");

    FILE *fp = fopen(head_path, "r");
    if (!fp)
        return -1;

    char buffer[1024];

    if (fscanf(fp, "ref: %s", buffer) != 1)
    {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    snprintf(ref_path_out, PATH_MAX, ".geg/%s", buffer);

    return 0;
}

char *get_parent_commit_id()
{

    char ref_path[PATH_MAX];
    if (get_head_ref_path(ref_path) != 0)
        return NULL;

    FILE *fp = fopen(ref_path, "r");
    if (!fp)
        return NULL;

    char *commit_id = malloc(41);
    if (fscanf(fp, "%40s", commit_id) != 1)
    {

        free(commit_id);
        fclose(fp);
        return NULL;
    }

    fclose(fp);
    return commit_id;
}

void update_head_ref(const char *new_commit_id)
{

    char ref_path[PATH_MAX];

    if (get_head_ref_path(ref_path) == 0)
    {

        FILE *fp = fopen(ref_path, "w");
        if (fp)
        {

            fprintf(fp, "%s\n", new_commit_id);
            fclose(fp);
        }
        else
        {

            perror("Failed to update HEAD ref");
        }
    }
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

void restore_blob(const char *path, const char *id)
{

    if (strcmp(path, "geg") == 0 || strcmp(path, "./geg") == 0)
    {
        return;
    }

    char obj_path[PATH_MAX];
    char temp_path[PATH_MAX];
    snprintf(obj_path, sizeof(obj_path), ".geg/objects/%.2s/%s", id, id + 2);
    snprintf(temp_path, sizeof(temp_path), ".geg/temp_checkout_%s", id);

    if (access(obj_path, F_OK) == -1)
    {
        printf("Error: Blob %s missing.\n", id);
        return;
    }

    decompress(obj_path, temp_path);

    FILE *src = fopen(temp_path, "rb");
    if (!src)
        return;

    char type[10];
    size_t size;
    if (fscanf(src, "%s %zu", type, &size) != 2)
    {
        fclose(src);
        return;
    }
    fgetc(src);

    FILE *dst = fopen(path, "wb");
    if (dst)
    {
        char buffer[1024];
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0)
        {
            fwrite(buffer, 1, n, dst);
        }
        fclose(dst);
    }
    else
    {
        perror("Failed to write workspace file");
    }

    fclose(src);
    remove(temp_path);
}

void restore_tree(const char *tree_id, const char *base_path)
{
    char obj_path[PATH_MAX];
    char temp_path[PATH_MAX];
    snprintf(obj_path, sizeof(obj_path), ".geg/objects/%.2s/%s", tree_id, tree_id + 2);
    snprintf(temp_path, sizeof(temp_path), ".geg/temp_tree_%s", tree_id);

    if (access(obj_path, F_OK) == -1)
        return;

    decompress(obj_path, temp_path);
    FILE *fp = fopen(temp_path, "rb");
    if (!fp)
        return;

    char type[10];
    size_t size;
    if (fscanf(fp, "%s %zu", type, &size) != 2 || strcmp(type, "tree") != 0)
    {
        fclose(fp);
        return;
    }
    fgetc(fp);

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
        while ((c = fgetc(fp)) != '\0' && c != EOF && i < 254)
            name[i++] = c;
        name[i] = '\0';

        if (fread(bin_sha, 1, 20, fp) != 20)
            break;
        for (int k = 0; k < 20; k++)
            sprintf(hex_sha + (k * 2), "%02x", bin_sha[k]);

        char full_path[PATH_MAX];
        if (base_path && strlen(base_path) > 0)
            snprintf(full_path, sizeof(full_path), "%s/%s", base_path, name);
        else
            snprintf(full_path, sizeof(full_path), "%s", name);

        if (strcmp(mode, "100644") == 0)
        {
            restore_blob(full_path, hex_sha);
        }
        else
        {
            mkdir(full_path, 0755);
            restore_tree(hex_sha, full_path);
        }
    }
    fclose(fp);
    remove(temp_path);
}

GegIndex *load_index()
{
    FILE *fp = fopen(".geg/index", "rb");
    if (!fp)
        return NULL;

    unsigned char header[12];
    if (fread(header, 1, 12, fp) != 12)
    {
        fclose(fp);
        return NULL;
    }

    if (memcmp(header, "DIRC", 4) != 0 || unpack32_be(header + 4) != 2)
    {
        printf("Error: Invalid or unsupported index format.\n");
        fclose(fp);
        return NULL;
    }

    GegIndex *index = malloc(sizeof(GegIndex));
    index->count = unpack32_be(header + 8);
    index->entries = malloc(sizeof(IndexEntry *) * index->count);

    for (uint32_t i = 0; i < index->count; i++)
    {
        unsigned char fixed[62];
        fread(fixed, 1, 62, fp);

        IndexEntry *entry = malloc(sizeof(IndexEntry));
        entry->ctime_sec = unpack32_be(fixed);
        entry->ctime_nsec = unpack32_be(fixed + 4);
        entry->mtime_sec = unpack32_be(fixed + 8);
        entry->mtime_nsec = unpack32_be(fixed + 12);
        entry->dev = unpack32_be(fixed + 16);
        entry->ino = unpack32_be(fixed + 20);
        entry->mode = unpack32_be(fixed + 24);
        entry->uid = unpack32_be(fixed + 28);
        entry->gid = unpack32_be(fixed + 32);
        entry->size = unpack32_be(fixed + 36);
        memcpy(entry->sha1, fixed + 40, 20);
        entry->flags = unpack16_be(fixed + 60);

        int name_len = entry->flags & 0xFFF;
        entry->path = malloc(name_len + 1);
        fread(entry->path, 1, name_len, fp);
        entry->path[name_len] = '\0';


        int padding = 8 - ((62 + name_len) % 8);
        fseek(fp, padding, SEEK_CUR);

        index->entries[i] = entry;
    }

    fclose(fp);
    return index;
}

void free_index(GegIndex *index)
{
    if (!index)
        return;
    for (uint32_t i = 0; i < index->count; i++)
    {
        free(index->entries[i]->path);
        free(index->entries[i]);
    }
    free(index->entries);
    free(index);
}

void get_commit_tree(const char *commit_id, char *tree_id_out)
{

    char obj_path[PATH_MAX];
    char temp_path[PATH_MAX];
    snprintf(obj_path, sizeof(obj_path), ".geg/objects/%.2s/%s", commit_id, commit_id + 2);
    snprintf(temp_path, sizeof(temp_path), ".geg/temp_read_%s", commit_id);

    if (access(obj_path, F_OK) == -1)
    {
        tree_id_out[0] = '\0';
        return;
    }

    decompress(obj_path, temp_path);
    FILE *fp = fopen(temp_path, "rb");

    if (!fp)
    {
        return;
    }

    char type[10];
    size_t size;
    fscanf(fp, "%s %zu", type, &size);
    fgetc(fp);

    char line[1024];

    if (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, "tree ", 5) == 0)
        {
            sscanf(line, "tree %40s", tree_id_out);
        }
    }

    fclose(fp);
    remove(temp_path);
}

void load_tree_entries(const char *tree_id, const char *base_path, HeadTree *head_tree)
{

    char obj_path[PATH_MAX];
    char temp_path[PATH_MAX];
    snprintf(obj_path, sizeof(obj_path), ".geg/objects/%.2s/%s", tree_id, tree_id + 2);
    snprintf(temp_path, sizeof(temp_path), ".geg/temp_tree_read_%s", tree_id);

    if (access(obj_path, F_OK) == -1)
    {
        return;
    }

    decompress(obj_path, temp_path);
    FILE *fp = fopen(temp_path, "rb");

    if (!fp)
    {
        return;
    }

    char type[10];
    size_t size;

    if (fscanf(fp, "%s %zu", type, &size) != 2 || strcmp(type, "tree") != 0)
    {
        fclose(fp);
        return;
    }

    fgetc(fp);

    long data_start = ftell(fp);

    while (ftell(fp) < data_start + size)
    {
        char mode[10];
        char name[255];
        unsigned char bin_sha[20];
        char hex_sha[41];

        if (fscanf(fp, "%s", mode) != 1)
        {
            break;
        }

        fgetc(fp);

        int i = 0;
        char c;

        while ((c = fgetc(fp)) != '\0' && c != EOF && i < 254)
        {
            name[i++] = c;
        }

        name[i] = '\0';

        if (fread(bin_sha, 1, 20, fp) != 20)
        {
            break;
        }

        for (int k = 0; k < 20; k++)
        {
            sprintf(hex_sha + (k * 2), "%02x", bin_sha[k]);
        }

        char full_path[PATH_MAX];

        if (base_path && strlen(base_path) > 0)
        {
            snprintf(full_path, sizeof(full_path), "%s/%s", base_path, name);
        }
        else
        {
            snprintf(full_path, sizeof(full_path), "%s", name);
        }

        if (strcmp(mode, "100644") == 0)
        {
            if (head_tree->count >= head_tree->capacity)
            {
                head_tree->capacity = head_tree->capacity == 0 ? 10 : head_tree->capacity * 2;
                head_tree->entries = realloc(head_tree->entries, sizeof(HeadEntry *) * head_tree->capacity);
            }

            HeadEntry *he = malloc(sizeof(HeadEntry));
            he->path = strdup(full_path);
            memcpy(he->sha1, bin_sha, 20);
            head_tree->entries[head_tree->count++] = he;
        }
        else
        {
            load_tree_entries(hex_sha, full_path, head_tree);
        }
    }

    fclose(fp);
    remove(temp_path);
}
