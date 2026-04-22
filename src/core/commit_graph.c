#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "../../include/core/commit_graph.h"
#include "../../include/core/object.h"

typedef struct {
    char **ids;
    int count, cap;
} CgIdSet;

typedef struct {
    char **data;
    int head, tail, cap;
} CgQueue;

static void cg_idset_init(CgIdSet *s)
{
    s->cap = 32;
    s->count = 0;
    s->ids = malloc(s->cap * sizeof(char *));
}

static int cg_idset_has(const CgIdSet *s, const char *id)
{
    for (int i = 0; i < s->count; i++)
        if (strcmp(s->ids[i], id) == 0)
            return 1;
    return 0;
}

static void cg_idset_add(CgIdSet *s, const char *id)
{
    if (cg_idset_has(s, id))
        return;
    if (s->count >= s->cap)
    {
        s->cap *= 2;
        s->ids = realloc(s->ids, s->cap * sizeof(char *));
    }
    s->ids[s->count++] = strdup(id);
}

static void cg_idset_free(CgIdSet *s)
{
    for (int i = 0; i < s->count; i++)
        free(s->ids[i]);
    free(s->ids);
}

static void cg_queue_init(CgQueue *q)
{
    q->cap = 64;
    q->head = q->tail = 0;
    q->data = malloc(q->cap * sizeof(char *));
}

static int cg_queue_empty(const CgQueue *q)
{
    return q->head == q->tail;
}

static void cg_queue_push(CgQueue *q, const char *id)
{
    if (q->tail >= q->cap)
    {
        q->cap *= 2;
        q->data = realloc(q->data, q->cap * sizeof(char *));
    }
    q->data[q->tail++] = strdup(id);
}

static char *cg_queue_pop(CgQueue *q)
{
    return q->data[q->head++];
}

static void cg_queue_drain(CgQueue *q)
{
    while (!cg_queue_empty(q))
    {
        char *x = cg_queue_pop(q);
        free(x);
    }
    free(q->data);
}

static void cg_add_entry(CommitGraph *cg, const char *oid)
{
    if (cg->count >= cg->capacity)
    {
        cg->capacity = cg->capacity == 0 ? 32 : cg->capacity * 2;
        cg->entries = realloc(cg->entries, cg->capacity * sizeof(CGraphEntry));
    }
    CGraphEntry *e = &cg->entries[cg->count++];
    strncpy(e->oid, oid, 40);
    e->oid[40] = '\0';
    e->num_parents = 0;
    e->parent_indices[0] = -1;
    e->parent_indices[1] = -1;
    e->generation = 0;
}

int cg_find(const CommitGraph *cg, const char *oid)
{
    for (int i = 0; i < cg->count; i++)
        if (strcmp(cg->entries[i].oid, oid) == 0)
            return i;
    return -1;
}

void cg_build(CommitGraph *cg)
{
    cg->entries = NULL;
    cg->count = 0;
    cg->capacity = 0;

    CgIdSet visited;
    cg_idset_init(&visited);
    CgQueue queue;
    cg_queue_init(&queue);

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
                cg_queue_push(&queue, oid);
            fclose(fp);
        }
        closedir(dir);
    }

    while (!cg_queue_empty(&queue))
    {
        char *cur = cg_queue_pop(&queue);
        if (!cg_idset_has(&visited, cur))
        {
            cg_idset_add(&visited, cur);
            cg_add_entry(cg, cur);

            char parents[2][41];
            int pc = 0;
            read_commit_parents(cur, parents, &pc);
            for (int i = 0; i < pc; i++)
                cg_queue_push(&queue, parents[i]);
        }
        free(cur);
    }
    cg_queue_drain(&queue);
    cg_idset_free(&visited);

    for (int i = 0; i < cg->count; i++)
    {
        char parents[2][41];
        int pc = 0;
        read_commit_parents(cg->entries[i].oid, parents, &pc);
        cg->entries[i].num_parents = pc;
        for (int p = 0; p < pc; p++)
            cg->entries[i].parent_indices[p] = cg_find(cg, parents[p]);
    }

    int *indegree = calloc(cg->count, sizeof(int));
    for (int i = 0; i < cg->count; i++)
        for (int p = 0; p < cg->entries[i].num_parents; p++)
            if (cg->entries[i].parent_indices[p] >= 0)
                indegree[cg->entries[i].parent_indices[p]]++;

    int *stack = malloc(cg->count * sizeof(int));
    int top = 0;
    for (int i = 0; i < cg->count; i++)
        if (indegree[i] == 0)
            stack[top++] = i;

    while (top > 0)
    {
        int idx = stack[--top];
        CGraphEntry *e = &cg->entries[idx];
        for (int p = 0; p < e->num_parents; p++)
        {
            int pi = e->parent_indices[p];
            if (pi < 0)
                continue;
            // gen = 1 + max(parent gens), propagating from tips toward roots
            if (e->generation < cg->entries[pi].generation + 1)
                e->generation = cg->entries[pi].generation + 1;
            indegree[pi]--;
            if (indegree[pi] == 0)
                stack[top++] = pi;
        }
    }

    free(indegree);
    free(stack);
}

void cg_free(CommitGraph *cg)
{
    free(cg->entries);
    cg->entries = NULL;
    cg->count = 0;
    cg->capacity = 0;
}
