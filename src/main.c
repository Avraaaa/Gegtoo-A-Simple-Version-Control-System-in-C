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

typedef struct
{
    uint32_t ctime_sec;
    uint32_t ctime_nsec;
    uint32_t mtime_sec;
    uint32_t mtime_nsec;
    uint32_t dev;
    uint32_t ino;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    unsigned char sha1[20];
    uint16_t flags;
    char *path;
} IndexEntry;

typedef struct
{
    IndexEntry **entries;
    uint32_t count;
} GegIndex;

typedef struct
{
    char *path;
    unsigned char sha1[20];
} HeadEntry;

typedef struct
{
    HeadEntry **entries;
    size_t count;
    size_t capacity;
} HeadTree;

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

        // Read the null-byte padding to advance the file pointer to the next entry
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

void geg_add(int argc, char *argv[])
{

    if (argc < 3)
    {
        printf("Usage: ./geg add <files...>\n");
        return;
    }

    FILE *index_file = fopen(".geg/index", "wb");

    if (!index_file)
    {
        perror("fopen");
        return;
    }

    size_t capacity = 4096;
    unsigned char *full_data = malloc(capacity);
    size_t offset = 0;

    unsigned char header[12];
    memcpy(header, "DIRC", 4);
    pack32_be(2, header + 4);
    pack32_be(0, header + 8);

    memcpy(full_data + offset, header, 12);
    offset += 12;

    int count = 0;

    for (int i = 2; i < argc; i++)
    {
        size_t size;
        char *content = read_workspace_files(argv[i], &size);

        if (content)
        {
            Blob blob;
            blob.data = content;
            blob.size = size;
            strcpy(blob.type, "blob");
            database_store(&blob);

            struct stat st;
            stat(argv[i], &st);

            unsigned char entry[62];
            pack32_be(st.st_ctime, entry);
            pack32_be(0, entry + 4);
            pack32_be(st.st_mtime, entry + 8);
            pack32_be(0, entry + 12);
            pack32_be(st.st_dev, entry + 16);
            pack32_be(st.st_ino, entry + 20);
            pack32_be(st.st_mode, entry + 24);
            pack32_be(st.st_uid, entry + 28);
            pack32_be(st.st_gid, entry + 32);
            pack32_be(size, entry + 36);

            unsigned char hash_bin[20];
            hex_to_binary(blob.id, hash_bin);
            memcpy(entry + 40, hash_bin, 20);

            unsigned int name_len = strlen(argv[i]);
            pack16_be(name_len & 0xFFF, entry + 60);

            if (offset + 62 + name_len + 8 + 20 > capacity)
            {
                capacity *= 2;
                full_data = realloc(full_data, capacity);
            }

            memcpy(full_data + offset, entry, 62);
            offset += 62;

            memcpy(full_data + offset, argv[i], name_len);
            offset += name_len;

            int padding = 8 - ((62 + name_len) % 8);
            memset(full_data + offset, 0, padding);
            offset += padding;

            count++;
            free(content);
        }
    }

    pack32_be(count, full_data + 8);

    unsigned char index_sha[20];
    sha1_hash(full_data, offset, index_sha);
    memcpy(full_data + offset, index_sha, 20);
    offset += 20;

    fwrite(full_data, 1, offset, index_file);
    fclose(index_file);
    free(full_data);
}

void geg_remove(int argc, char *argv[])
{

    if (argc < 3)
    {
        printf("Usage: ./geg remove [--cached] <files...>\n");
        return;
    }

    int cached = 0;
    int start_idx = 2;

    if (strcmp(argv[2], "--cached") == 0)
    {
        cached = 1;
        start_idx = 3;
    }

    if (start_idx >= argc)
    {
        return;
    }

    GegIndex *index = load_index();

    if (!index)
    {
        return;
    }

    FILE *index_file = fopen(".geg/index", "wb");

    if (!index_file)
    {
        perror("fopen");
        free_index(index);
        return;
    }

    size_t capacity = 4096;
    unsigned char *full_data = malloc(capacity);
    size_t offset = 0;

    unsigned char header[12];
    memcpy(header, "DIRC", 4);
    pack32_be(2, header + 4);
    pack32_be(0, header + 8);

    memcpy(full_data + offset, header, 12);
    offset += 12;

    int kept_count = 0;

    for (uint32_t i = 0; i < index->count; i++)
    {
        IndexEntry *ie = index->entries[i];
        int to_remove = 0;

        for (int j = start_idx; j < argc; j++)
        {
            if (strcmp(ie->path, argv[j]) == 0)
            {
                to_remove = 1;
                break;
            }
        }

        if (to_remove)
        {
            if (!cached)
            {
                remove(ie->path);
            }
            continue;
        }

        unsigned char entry_buf[62];
        pack32_be(ie->ctime_sec, entry_buf);
        pack32_be(ie->ctime_nsec, entry_buf + 4);
        pack32_be(ie->mtime_sec, entry_buf + 8);
        pack32_be(ie->mtime_nsec, entry_buf + 12);
        pack32_be(ie->dev, entry_buf + 16);
        pack32_be(ie->ino, entry_buf + 20);
        pack32_be(ie->mode, entry_buf + 24);
        pack32_be(ie->uid, entry_buf + 28);
        pack32_be(ie->gid, entry_buf + 32);
        pack32_be(ie->size, entry_buf + 36);
        memcpy(entry_buf + 40, ie->sha1, 20);
        pack16_be(ie->flags, entry_buf + 60);

        unsigned int name_len = strlen(ie->path);

        if (offset + 62 + name_len + 8 + 20 > capacity)
        {
            capacity *= 2;
            full_data = realloc(full_data, capacity);
        }

        memcpy(full_data + offset, entry_buf, 62);
        offset += 62;

        memcpy(full_data + offset, ie->path, name_len);
        offset += name_len;

        int padding = 8 - ((62 + name_len) % 8);
        memset(full_data + offset, 0, padding);
        offset += padding;

        kept_count++;
    }

    pack32_be(kept_count, full_data + 8);

    unsigned char index_sha[20];
    sha1_hash(full_data, offset, index_sha);
    memcpy(full_data + offset, index_sha, 20);
    offset += 20;

    fwrite(full_data, 1, offset, index_file);
    fclose(index_file);
    free(full_data);
    free_index(index);
}

void geg_status(void)
{

    char root_path[PATH_MAX];

    if (getcwd(root_path, sizeof(root_path)) == NULL)
    {
        perror("getcwd");
        return;
    }

    int count = 0;
    char **files = list_workspace_files(root_path, &count);

    GegIndex *index = load_index();

    printf("Changes to be committed:\n");
    printf("  (use \"geg remove --staged <file>...\" to unstage)\n\n");

    int has_staged = 0;

    char *head_commit = get_parent_commit_id();
    HeadTree head_tree = {NULL, 0, 0};

    if (head_commit)
    {
        char tree_id[41] = {0};
        get_commit_tree(head_commit, tree_id);

        if (strlen(tree_id) > 0)
        {
            load_tree_entries(tree_id, "", &head_tree);
        }

        free(head_commit);
    }

    if (index)
    {
        for (uint32_t i = 0; i < index->count; i++)
        {
            IndexEntry *ie = index->entries[i];
            int found_in_head = 0;

            for (size_t j = 0; j < head_tree.count; j++)
            {
                if (strcmp(ie->path, head_tree.entries[j]->path) == 0)
                {
                    found_in_head = 1;

                    if (memcmp(ie->sha1, head_tree.entries[j]->sha1, 20) != 0)
                    {
                        printf("    modified:   %s\n", ie->path);
                        has_staged = 1;
                    }

                    break;
                }
            }

            if (!found_in_head)
            {
                printf("    new file:   %s\n", ie->path);
                has_staged = 1;
            }
        }
    }

    for (size_t j = 0; j < head_tree.count; j++)
    {
        int found_in_index = 0;

        if (index)
        {
            for (uint32_t i = 0; i < index->count; i++)
            {
                if (strcmp(head_tree.entries[j]->path, index->entries[i]->path) == 0)
                {
                    found_in_index = 1;
                    break;
                }
            }
        }

        if (!found_in_index)
        {
            printf("    deleted:    %s\n", head_tree.entries[j]->path);
            has_staged = 1;
        }

        free(head_tree.entries[j]->path);
        free(head_tree.entries[j]);
    }

    if (head_tree.entries)
    {
        free(head_tree.entries);
    }

    if (!has_staged)
    {
        printf("    (none)\n");
    }

    printf("\nChanges not staged for commit:\n");
    printf("  (use \"geg add <file>...\" to update what will be committed)\n");
    printf("  (use \"geg remove <file>...\" to discard changes in working directory)\n\n");

    int has_modified = 0;

    if (index)
    {
        for (uint32_t i = 0; i < index->count; i++)
        {
            IndexEntry *entry = index->entries[i];
            struct stat st;

            if (stat(entry->path, &st) == 0)
            {
                int modified = 0;

                if (entry->size != st.st_size)
                {
                    modified = 1;
                }
                else if (entry->mtime_sec != (uint32_t)st.st_mtime)
                {
                    size_t size;
                    char *content = read_workspace_files(entry->path, &size);

                    if (content)
                    {
                        char header[64];
                        int header_len = snprintf(header, sizeof(header), "blob %zu", size);
                        size_t full_len = header_len + 1 + size;
                        unsigned char *full_data = malloc(full_len);

                        memcpy(full_data, header, header_len);
                        full_data[header_len] = '\0';
                        memcpy(full_data + header_len + 1, content, size);

                        unsigned char hash_bin[20];
                        sha1_hash(full_data, full_len, hash_bin);

                        free(full_data);
                        free(content);

                        if (memcmp(entry->sha1, hash_bin, 20) != 0)
                        {
                            modified = 1;
                        }
                    }
                }

                if (modified)
                {
                    printf("    modified:   %s\n", entry->path);
                    has_modified = 1;
                }
            }
            else
            {
                printf("    deleted:    %s\n", entry->path);
                has_modified = 1;
            }
        }
    }

    if (!has_modified)
    {
        printf("    (none)\n");
    }

    printf("\nUntracked files:\n");
    printf("  (use \"geg add <file>...\" to include files to be committed)\n\n");

    int has_untracked = 0;

    for (int i = 0; i < count; i++)
    {
        int is_tracked = 0;

        if (index)
        {
            for (uint32_t j = 0; j < index->count; j++)
            {
                if (strcmp(files[i], index->entries[j]->path) == 0)
                {
                    is_tracked = 1;
                    break;
                }
            }
        }

        if (!is_tracked)
        {
            printf("    %s\n", files[i]);
            has_untracked = 1;
        }

        free(files[i]);
    }

    if (!has_untracked)
    {
        printf("    (none)\n");
    }

    if (files)
    {
        free(files);
    }

    if (index)
    {
        free_index(index);
    }
}

void geg_commit(void)
{

    GegIndex *index = load_index();

    if (!index)
    {
        printf("nothing to commit\n");
        if (index)
        {
            free_index(index);
        }
        return;
    }

    Tree tree;
    tree.count = index->count;
    tree.entries = malloc(sizeof(Entry *) * tree.count);

    for (uint32_t i = 0; i < index->count; i++)
    {
        char hex_sha[41];

        for (int k = 0; k < 20; k++)
        {
            sprintf(hex_sha + (k * 2), "%02x", index->entries[i]->sha1[k]);
        }

        tree.entries[i] = new_entry(index->entries[i]->path, hex_sha);
    }

    size_t tree_size = 0;
    unsigned char *tree_data = serialize_tree(&tree, &tree_size);
    char tree_id[41] = {0};

    if (tree_data)
    {
        Blob tree_blob;
        tree_blob.data = (char *)tree_data;
        tree_blob.size = tree_size;
        strcpy(tree_blob.type, "tree");

        database_store(&tree_blob);
        strcpy(tree_id, tree_blob.id);

        free(tree_data);
    }

    for (size_t i = 0; i < tree.count; i++)
    {
        free_entry(tree.entries[i]);
    }

    free(tree.entries);
    free_index(index);

    char *parent_id = get_parent_commit_id();

    time_t now = time(NULL);
    struct tm local_tm = *localtime(&now);
    struct tm gmt_tm = *gmtime(&now);

    gmt_tm.tm_isdst = local_tm.tm_isdst;

    time_t local_time = mktime(&local_tm);
    time_t gmt_time = mktime(&gmt_tm);

    long offset = (long)difftime(local_time, gmt_time);

    char sign = '+';

    if (offset < 0)
    {
        sign = '-';
        offset = -offset;
    }

    int hours = offset / 3600;
    int mins = (offset % 3600) / 60;

    char time_str[32];
    sprintf(time_str, "%ld %c%02d%02d", (long)now, sign, hours, mins);
    char *author = "Geg User <geg@gmail.com>";
    char *message = "Automatic commit by geg";

    size_t commit_size = 1024 + (parent_id ? 50 : 0);
    char *commit_content = malloc(commit_size);

    int content_offset = sprintf(commit_content, "tree %s\n", tree_id);

    if (parent_id)
    {
        content_offset += sprintf(commit_content + content_offset, "parent %s\n", parent_id);
    }

    content_offset += sprintf(commit_content + content_offset,
                      "author %s %s\n"
                      "committer %s %s\n"
                      "\n"
                      "%s\n",
                      author, time_str, author, time_str, message);

    Blob commit_blob;
    commit_blob.data = commit_content;
    commit_blob.size = content_offset;
    strcpy(commit_blob.type, "commit");

    database_store(&commit_blob);

    printf("[%s%s] %s\n", commit_blob.id, parent_id ? "" : " (root-commit)", message);

    update_head_ref(commit_blob.id);

    free(commit_content);
    if (parent_id)
    {
        free(parent_id);
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

void geg_log(void)
{
    char ref_path[PATH_MAX];
    if (get_head_ref_path(ref_path) != 0)
    {
        printf("HEAD not found(not a geg repository?)\n");
        return;
    }

    FILE *fp_ref = fopen(ref_path, "r");

    if (!fp_ref)
    {
        printf("No Commits yet.\n");
        return;
    }

    char current_id[41];
    if (fscanf(fp_ref, "%40s", current_id) != 1)
    {
        fclose(fp_ref);
        return;
    }

    fclose(fp_ref);

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

void geg_checkout(const char *target_id)
{

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