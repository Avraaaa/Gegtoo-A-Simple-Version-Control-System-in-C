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
#include "../../include/core/commit_graph.h"
#include "../../include/utils/huffman.h"
#include "../../include/utils/kahn.h"
#include "../../include/commands.h"

#define COLOR_RESET   "\033[0m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_CYAN    "\033[36m"

typedef struct {
    char oid[41];
    char **labels;
    int label_count;
    int head_branch; // index into labels that HEAD points to, -1 if none
} RefDeco;

static RefDeco *s_decos = NULL;
static int s_deco_count = 0;
static int s_deco_cap = 0;

static RefDeco *find_deco(const char *oid)
{
    for (int i = 0; i < s_deco_count; i++)
        if (strcmp(s_decos[i].oid, oid) == 0)
            return &s_decos[i];
    return NULL;
}

static RefDeco *get_or_create_deco(const char *oid)
{
    RefDeco *d = find_deco(oid);
    if (d)
        return d;

    if (s_deco_count >= s_deco_cap)
    {
        s_deco_cap = s_deco_cap == 0 ? 16 : s_deco_cap * 2;
        s_decos = realloc(s_decos, s_deco_cap * sizeof(RefDeco));
    }
    d = &s_decos[s_deco_count++];
    strncpy(d->oid, oid, 40);
    d->oid[40] = '\0';
    d->labels = NULL;
    d->label_count = 0;
    d->head_branch = -1;
    return d;
}

static void add_label(RefDeco *d, const char *label)
{
    d->labels = realloc(d->labels, (d->label_count + 1) * sizeof(char *));
    d->labels[d->label_count++] = strdup(label);
}

static void build_decorations(void)
{
    s_decos = NULL;
    s_deco_count = 0;
    s_deco_cap = 0;

    char head_branch[PATH_MAX] = {0};

    FILE *hfp = fopen(".geg/HEAD", "r");
    if (hfp)
    {
        char buf[1024];
        if (fscanf(hfp, "ref: refs/heads/%s", buf) == 1)
            strcpy(head_branch, buf);
        fclose(hfp);
    }

    DIR *dir = opendir(".geg/refs/heads");
    if (dir)
    {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL)
        {
            if (ent->d_name[0] == '.')
                continue;
            char path[PATH_MAX];
            snprintf(path, sizeof(path), ".geg/refs/heads/%s", ent->d_name);
            FILE *fp = fopen(path, "r");
            if (!fp)
                continue;
            char oid[41] = {0};
            if (fscanf(fp, "%40s", oid) == 1)
            {
                RefDeco *d = get_or_create_deco(oid);
                add_label(d, ent->d_name);
                if (strcmp(ent->d_name, head_branch) == 0)
                    d->head_branch = d->label_count - 1;
            }
            fclose(fp);
        }
        closedir(dir);
    }

    dir = opendir(".geg/refs/tags");
    if (dir)
    {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL)
        {
            if (ent->d_name[0] == '.')
                continue;
            char path[PATH_MAX];
            snprintf(path, sizeof(path), ".geg/refs/tags/%s", ent->d_name);
            FILE *fp = fopen(path, "r");
            if (!fp)
                continue;
            char oid[41] = {0};
            if (fscanf(fp, "%40s", oid) == 1)
            {
                char tag_label[PATH_MAX];
                snprintf(tag_label, sizeof(tag_label), "tag: %s", ent->d_name);
                RefDeco *d = get_or_create_deco(oid);
                add_label(d, tag_label);
            }
            fclose(fp);
        }
        closedir(dir);
    }
}

static void free_decorations(void)
{
    for (int i = 0; i < s_deco_count; i++)
    {
        for (int j = 0; j < s_decos[i].label_count; j++)
            free(s_decos[i].labels[j]);
        free(s_decos[i].labels);
    }
    free(s_decos);
    s_decos = NULL;
    s_deco_count = 0;
    s_deco_cap = 0;
}

static void print_deco_string(const char *oid)
{
    RefDeco *d = find_deco(oid);
    if (!d || d->label_count == 0)
        return;

    printf(" " COLOR_BOLD COLOR_CYAN "(");

    for (int i = 0; i < d->label_count; i++)
    {
        if (i == d->head_branch)
            printf("HEAD -> ");
        printf("%s", d->labels[i]);
        if (i < d->label_count - 1)
            printf(", ");
    }

    printf(")" COLOR_RESET);
}

static void print_commit_body(const char *oid, const char *prefix, FILE *fp)
{
    char line[1024];
    char author[255] = {0};

    while (fgets(line, sizeof(line), fp))
    {
        if (line[0] == '\n')
            break;
        if (strncmp(line, "author ", 7) == 0)
        {
            char *p = line + 7;
            size_t len = strlen(p);
            if (p[len - 1] == '\n')
                p[len - 1] = 0;
            strcpy(author, p);
        }
    }

    printf("%s" COLOR_YELLOW "commit %s" COLOR_RESET, prefix, oid);
    print_deco_string(oid);
    printf("\n");

    printf("%s" COLOR_GREEN "Author: %s" COLOR_RESET "\n", prefix, author);
    printf("%s\n", prefix);

    while (fgets(line, sizeof(line), fp))
    {
        printf("%s    %s", prefix, line);
    }
    printf("\n");
}

static void print_commit_graph(const char *oid, const char *graph_prefix)
{
    char obj_path[PATH_MAX];
    char temp_path[PATH_MAX];

    snprintf(obj_path, sizeof(obj_path), ".geg/objects/%.2s/%s", oid, oid + 2);
    snprintf(temp_path, sizeof(temp_path), ".geg/temp_log_%s", oid);

    if (access(obj_path, F_OK) == -1)
        return;

    decompress(obj_path, temp_path);
    FILE *fp = fopen(temp_path, "rb");
    if (!fp)
        return;

    char type[10];
    size_t size;
    if (fscanf(fp, "%s %zu", type, &size) != 2 || strcmp(type, "commit"))
    {
        fclose(fp);
        remove(temp_path);
        return;
    }
    fgetc(fp);

    print_commit_body(oid, graph_prefix, fp);

    fclose(fp);
    remove(temp_path);
}

static void plain_log(const char *start_oid)
{
    char current_id[41];
    strcpy(current_id, start_oid);

    while (1)
    {
        char obj_path[PATH_MAX];
        char temp_path[PATH_MAX];

        snprintf(obj_path, sizeof(obj_path), ".geg/objects/%.2s/%s", current_id, current_id + 2);
        snprintf(temp_path, sizeof(temp_path), ".geg/temp_log_%s", current_id);

        if (access(obj_path, F_OK) == -1)
            break;

        decompress(obj_path, temp_path);
        FILE *fp = fopen(temp_path, "rb");
        if (!fp)
            break;

        char type[10];
        size_t size;
        if (fscanf(fp, "%s %zu", type, &size) != 2 || strcmp(type, "commit"))
        {
            fclose(fp);
            remove(temp_path);
            break;
        }
        fgetc(fp);

        // need to grab parent before print_commit_body consumes the headers
        long header_start = ftell(fp);
        char line[1024];
        char parent_id[41] = {0};
        while (fgets(line, sizeof(line), fp))
        {
            if (line[0] == '\n')
                break;
            if (strncmp(line, "parent ", 7) == 0)
                sscanf(line, "parent %40s", parent_id);
        }
        fseek(fp, header_start, SEEK_SET);

        print_commit_body(current_id, "", fp);

        fclose(fp);
        remove(temp_path);

        if (strlen(parent_id) > 0)
            strcpy(current_id, parent_id);
        else
            break;
    }
}

void geg_log(int argc, char *argv[])
{
    char head_oid[41];

    if (resolve_ref("HEAD", head_oid) != 0)
    {
        printf("HEAD not found or no commits yet. :(\n");
        return;
    }

    build_decorations();

    int graph_mode = 0;
    for (int i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], "--graph") == 0)
            graph_mode = 1;
    }

    if (graph_mode)
    {
        CommitGraph cg;
        cg_build(&cg);

        if (cg.count == 0)
        {
            free_decorations();
            printf("HEAD not found or no commits yet. :(\n");
            return;
        }

        kahn_log(&cg, print_commit_graph);
        cg_free(&cg);
    }
    else
    {
        plain_log(head_oid);
    }

    free_decorations();
}
