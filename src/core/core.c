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

int compare_Index_Entries_by_path(const void *a, const void *b)
{

    IndexEntry *ea = *(IndexEntry **)a;
    IndexEntry *eb = *(IndexEntry **)b;
    return strcmp(ea->path, eb->path);
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

    // if (stat(name, &path_stat) == 0 && S_ISDIR(path_stat.st_mode))
    // {
    //     return 1;
    // }
    return 0;
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

int resolve_ref(const char *refname, char *hash_out)
{

    char path[PATH_MAX];

    // Default  is head
    if (strcmp(refname, "HEAD") == 0)
    {
        snprintf(path, sizeof(path), ".geg/HEAD");
    }

    // Leaves room for a different branch
    else
    {
        snprintf(path, sizeof(path), ".geg/refs/heads/%s", refname);
    }

    FILE *fp = fopen(path, "r");
    if (!fp)
    {
        return -1;
    }

    char buffer[1024];

    if (fscanf(fp, "%s", buffer) != 1)
    {
        fclose(fp);
        return -1;
    }

    // Attached
    if (strcmp(buffer, "ref:") == 0)
    {
        char ref_path[PATH_MAX];
        fscanf(fp, "%s", ref_path);
        fclose(fp);

        char full_ref_path[PATH_MAX];
        snprintf(full_ref_path, sizeof(full_ref_path), ".geg/%s", ref_path);

        FILE *ref_fp = fopen(full_ref_path, "r");

        if (!ref_fp)
        {
            return -1;
        }

        if (fscanf(ref_fp, "%40s", hash_out) != 1)
        {
            fclose(ref_fp);
            return -1;
        }

        fclose(ref_fp);
        return 0;
    }
    else
    {
        strncpy(hash_out, buffer, 41);
        fclose(fp);
        return 0;
    }
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

    char *commit_id = malloc(41);
    if (resolve_ref("HEAD", commit_id) == 0)
    {
        return commit_id;
    }

    free(commit_id);
    return NULL;
}

void update_head_ref(const char *new_commit_id)
{
    char head_path[PATH_MAX];
    snprintf(head_path, sizeof(head_path), ".geg/HEAD");

    FILE *fp = fopen(head_path, "r");
    if (!fp)
    {
        perror("Failed to read HEAD");
        return;
    }

    char buffer[1024];
    if (fscanf(fp, "%s", buffer) != 1)
    {
        fclose(fp);
        return;
    }

    if (strcmp(buffer, "ref:") == 0)
    {
        //Attached, update branch file
        char ref_path[PATH_MAX];
        fscanf(fp, "%s", ref_path);
        fclose(fp);

        char full_ref_path[PATH_MAX];
        snprintf(full_ref_path, sizeof(full_ref_path), ".geg/%s", ref_path);

        FILE *ref_fp = fopen(full_ref_path, "w");
        if (ref_fp)
        {
            fprintf(ref_fp, "%s\n", new_commit_id);
            fclose(ref_fp);
        }
        else
        {
            perror("Failed to update branch ref");
        }
    }
    else
    {
        // WDetached, overwrite raw hash
        fclose(fp); 

        FILE *head_out = fopen(head_path, "w");
        if (head_out)
        {
            fprintf(head_out, "%s\n", new_commit_id);
            fclose(head_out);
        }
        else
        {
            perror("Failed to update detached HEAD");
        }
    }
}

void explore_directory(const char *base_path, const char *relative_path, char ***file_list, int *count)
{

    char current_dir_path[PATH_MAX];
    // use base path if root, else append relative path
    if (strlen(relative_path) == 0)
    {
        snprintf(current_dir_path, sizeof(current_dir_path), "%s", base_path);
    }
    else
    {
        snprintf(current_dir_path, sizeof(current_dir_path), "%s/%s", base_path, relative_path);
    }

    DIR *dir = opendir(current_dir_path);
    if (!dir)
        return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {

        if (is_ignored(entry->d_name))
            continue;

        char new_relative_path[PATH_MAX];
        // build new relative path
        if (strlen(relative_path) == 0)
        {
            snprintf(new_relative_path, sizeof(new_relative_path), "%s", entry->d_name);
        }
        else
        {
            snprintf(new_relative_path, sizeof(new_relative_path), "%s/%s", relative_path, entry->d_name);
        }

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, new_relative_path);

        struct stat st;
        if (stat(full_path, &st) == 0)
        {

            if (S_ISDIR(st.st_mode))
            {
                // If the path is a folder, recursively call it again to explore subdirectories and files
                explore_directory(base_path, new_relative_path, file_list, count);
            }

            else if (S_ISREG(st.st_mode))
            {
                char **temp = realloc(*file_list, sizeof(char *) * (*count + 1));
                if (temp)
                {
                    *file_list = temp;
                    (*file_list)[*count] = strdup(new_relative_path);
                    (*count)++;
                }
            }
        }
    }
    closedir(dir);
}

char **list_workspace_files(const char *root_path, int *count)
{
    char **file_list = NULL;
    *count = 0;
    explore_directory(root_path, "", &file_list, count);
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


static char** split_lines(char* content, size_t size, int* lines_count) {
    if (size == 0) {
        *lines_count = 0;
        return NULL;
    }
    int count = 1;
    for (size_t i = 0; i < size; i++) {
        if (content[i] == '\n') count++;
    }
    char** lines = malloc(count * sizeof(char*));
    int idx = 0;
    size_t start = 0;
    for (size_t i = 0; i <= size; i++) {
        if (i == size || content[i] == '\n') {
            size_t len = i - start;
            lines[idx] = malloc(len + 2);
            memcpy(lines[idx], content + start, len);
            if (i < size) {
                lines[idx][len] = '\n';
                lines[idx][len + 1] = '\0';
            } else {
                lines[idx][len] = '\0';
            }
            idx++;
            start = i + 1;
        }
    }
    *lines_count = idx;
    return lines;
}

static void myers_diff(char** A, int N, char** B, int M) {
    int max_d = N + M;
    if (max_d == 0) return;
    int* V = malloc((2 * max_d + 1) * sizeof(int));
    int** trace = malloc((max_d + 1) * sizeof(int*));
    for (int i = 0; i <= max_d; i++) {
        trace[i] = malloc((2 * max_d + 1) * sizeof(int));
    }

    V[max_d + 1] = 0;
    int d = 0, k = 0, x = 0, y = 0;
    int found = 0;

    for (d = 0; d <= max_d; d++) {
        for (k = -d; k <= d; k += 2) {
            int down = (k == -d || (k != d && V[max_d + k - 1] < V[max_d + k + 1]));
            int kPrev = down ? k + 1 : k - 1;

            if (down) {
                x = V[max_d + kPrev];
            } else {
                x = V[max_d + kPrev] + 1;
            }
            y = x - k;

            while (x < N && y < M && strcmp(A[x], B[y]) == 0) {
                x++;
                y++;
            }

            V[max_d + k] = x;
            trace[d][max_d + k] = x;

            if (x >= N && y >= M) {
                found = 1;
                break;
            }
        }
        if (found) break;
    }

    if (!found) {
        for (int i = 0; i <= max_d; i++) free(trace[i]);
        free(trace);
        free(V);
        return;
    }

    EditItem* script = malloc(d * sizeof(EditItem));
    int script_len = 0;
    x = N;
    y = M;

    for (int cur_d = d; cur_d > 0; cur_d--) {
        int cur_k = x - y;
        int down = (cur_k == -cur_d || (cur_k != cur_d && trace[cur_d - 1][max_d + cur_k - 1] < trace[cur_d - 1][max_d + cur_k + 1]));
        int kPrev = down ? cur_k + 1 : cur_k - 1;
        int prev_x = trace[cur_d - 1][max_d + kPrev];
        int prev_y = prev_x - kPrev;

        while (x > prev_x && y > prev_y) {
            x--;
            y--;
        }

        script[script_len].prev_x = prev_x;
        script[script_len].prev_y = prev_y;
        script[script_len].x = x;
        script[script_len].y = y;
        script_len++;

        x = prev_x;
        y = prev_y;
    }

    int cx = 0, cy = 0;
    for (int i = script_len - 1; i >= 0; i--) {
        while (cx < script[i].prev_x && cy < script[i].prev_y) {
            printf(" %s", A[cx]);
            if (A[cx][strlen(A[cx])-1] != '\n') printf("\n");
            cx++;
            cy++;
        }
        if (script[i].x > script[i].prev_x) {
            printf("\033[31m-%s\033[0m", A[script[i].prev_x]);
            if (A[script[i].prev_x][strlen(A[script[i].prev_x])-1] != '\n') printf("\n");
            cx++;
        } else if (script[i].y > script[i].prev_y) {
            printf("\033[32m+%s\033[0m", B[script[i].prev_y]);
            if (B[script[i].prev_y][strlen(B[script[i].prev_y])-1] != '\n') printf("\n");
            cy++;
        }
    }

    while (cx < N && cy < M) {
        printf(" %s", A[cx]);
        if (A[cx][strlen(A[cx])-1] != '\n') printf("\n");
        cx++;
        cy++;
    }

    free(script);
    for (int i = 0; i <= max_d; i++) free(trace[i]);
    free(trace);
    free(V);
}

void diff_file(const char* filepath, const unsigned char* old_sha1) {
    char obj_path[PATH_MAX];
    char temp_path[PATH_MAX];
    char hex_sha1[41];
    
    for (int i = 0; i < 20; i++) {
        sprintf(hex_sha1 + (i * 2), "%02x", old_sha1[i]);
    }
    
    snprintf(obj_path, sizeof(obj_path), ".geg/objects/%.2s/%s", hex_sha1, hex_sha1 + 2);
    snprintf(temp_path, sizeof(temp_path), ".geg/temp_diff_%s", hex_sha1);

    if (access(obj_path, F_OK) == -1) {
        return;
    }

    decompress(obj_path, temp_path);

    FILE* fp = fopen(temp_path, "rb");
    if (!fp) return;

    char type[10];
    size_t old_size;
    if (fscanf(fp, "%s %zu", type, &old_size) != 2) {
        fclose(fp);
        remove(temp_path);
        return;
    }
    fgetc(fp);

    char* old_content = NULL;
    if (old_size > 0) {
        old_content = malloc(old_size);
        fread(old_content, 1, old_size, fp);
    }
    fclose(fp);
    remove(temp_path);

    size_t new_size = 0;
    char* new_content = read_workspace_files(filepath, &new_size);

    int old_lines_count = 0, new_lines_count = 0;
    char** old_A = split_lines(old_content, old_size, &old_lines_count);
    char** new_B = split_lines(new_content, new_size, &new_lines_count);

    printf("diff --geg a/%s b/%s\n", filepath, filepath);
    printf("--- a/%s\n", filepath);
    printf("+++ b/%s\n", filepath);
    myers_diff(old_A, old_lines_count, new_B, new_lines_count);

    for (int i = 0; i < old_lines_count; i++) free(old_A[i]);
    free(old_A);
    for (int i = 0; i < new_lines_count; i++) free(new_B[i]);
    free(new_B);
    if (old_content) free(old_content);
    if (new_content) free(new_content);
}
