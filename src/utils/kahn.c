#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/utils/kahn.h"

#define ANSI_RED          "\033[31m"
#define ANSI_GREEN        "\033[32m"
#define ANSI_YELLOW       "\033[33m"
#define ANSI_BLUE         "\033[34m"
#define ANSI_MAGENTA      "\033[35m"
#define ANSI_CYAN         "\033[36m"

#define ANSI_BRIGHT_RED     "\033[91m"
#define ANSI_BRIGHT_GREEN   "\033[92m"
#define ANSI_BRIGHT_YELLOW  "\033[93m"
#define ANSI_BRIGHT_BLUE    "\033[94m"
#define ANSI_BRIGHT_MAGENTA "\033[95m"
#define ANSI_BRIGHT_CYAN    "\033[96m"

#define ANSI_RESET        "\033[0m"

static const char *LANE_COLORS[] = {
    ANSI_YELLOW,
    ANSI_BRIGHT_CYAN,
    ANSI_MAGENTA,
    ANSI_BRIGHT_GREEN,
    ANSI_RED,
    ANSI_BRIGHT_BLUE,
    ANSI_BRIGHT_YELLOW,
    ANSI_CYAN,
    ANSI_BRIGHT_MAGENTA,
    ANSI_GREEN,
    ANSI_BRIGHT_RED,
    ANSI_BLUE
};
#define NUM_LANE_COLORS 12

typedef struct {
    int *data;
    int count, cap;
} MaxHeap;

static void heap_init(MaxHeap *h)
{
    h->cap = 32;
    h->count = 0;
    h->data = malloc(h->cap * sizeof(int));
}

static void heap_swap(int *a, int *b)
{
    int t = *a;
    *a = *b;
    *b = t;
}

static void heap_push(MaxHeap *h, int idx, const CommitGraph *cg)
{
    if (h->count >= h->cap)
    {
        h->cap *= 2;
        h->data = realloc(h->data, h->cap * sizeof(int));
    }
    h->data[h->count] = idx;
    int i = h->count++;

    while (i > 0)
    {
        int parent = (i - 1) / 2;
        if (cg->entries[h->data[i]].generation > cg->entries[h->data[parent]].generation)
        {
            heap_swap(&h->data[i], &h->data[parent]);
            i = parent;
        }
        else
            break;
    }
}

static int heap_pop(MaxHeap *h, const CommitGraph *cg)
{
    int result = h->data[0];
    h->data[0] = h->data[--h->count];
    int i = 0;

    while (1)
    {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        int largest = i;

        if (left < h->count &&
            cg->entries[h->data[left]].generation > cg->entries[h->data[largest]].generation)
            largest = left;
        if (right < h->count &&
            cg->entries[h->data[right]].generation > cg->entries[h->data[largest]].generation)
            largest = right;

        if (largest != i)
        {
            heap_swap(&h->data[i], &h->data[largest]);
            i = largest;
        }
        else
            break;
    }
    return result;
}

static void heap_free(MaxHeap *h)
{
    free(h->data);
}

static const char *lane_color(int lane)
{
    return LANE_COLORS[lane % NUM_LANE_COLORS];
}

void kahn_log(const CommitGraph *cg, CommitPrinter printer)
{
    if (cg->count == 0)
        return;

    int *indegree = calloc(cg->count, sizeof(int));

    for (int i = 0; i < cg->count; i++)
        for (int p = 0; p < cg->entries[i].num_parents; p++)
            if (cg->entries[i].parent_indices[p] >= 0)
                indegree[cg->entries[i].parent_indices[p]]++;

    MaxHeap heap;
    heap_init(&heap);

    for (int i = 0; i < cg->count; i++)
        if (indegree[i] == 0)
            heap_push(&heap, i, cg);

    int lane_cap = 8;
    int lane_count = 0;
    int *lanes = malloc(lane_cap * sizeof(int));

    while (heap.count > 0)
    {
        int idx = heap_pop(&heap, cg);
        const CGraphEntry *e = &cg->entries[idx];

        int my_lane = -1;
        for (int l = 0; l < lane_count; l++)
        {
            if (lanes[l] == idx)
            {
                my_lane = l;
                break;
            }
        }

        if (my_lane == -1)
        {
            if (lane_count >= lane_cap)
            {
                lane_cap *= 2;
                lanes = realloc(lanes, lane_cap * sizeof(int));
            }
            my_lane = lane_count;
            lanes[lane_count++] = idx;
        }

        // building colored graph prefix
        char prefix[512] = {0};
        int pos = 0;
        for (int l = 0; l < lane_count && pos < 480; l++)
        {
            const char *c = lane_color(l);
            if (l == my_lane)
                pos += snprintf(prefix + pos, 512 - pos, "%s*%s ", c, ANSI_RESET);
            else
                pos += snprintf(prefix + pos, 512 - pos, "%s|%s ", c, ANSI_RESET);
        }

        printer(e->oid, prefix);

        if (e->num_parents == 2)
        {
            int p1_idx = e->parent_indices[1];
            if (p1_idx >= 0)
            {
                if (lane_count >= lane_cap)
                {
                    lane_cap *= 2;
                    lanes = realloc(lanes, lane_cap * sizeof(int));
                }
                lanes[lane_count++] = p1_idx;

                // merge fork connector
                for (int l = 0; l < lane_count; l++)
                {
                    if (l == my_lane)
                        printf("%s|\\%s", lane_color(l), ANSI_RESET);
                    else if (l == lane_count - 1)
                        break;
                    else
                        printf("%s|%s ", lane_color(l), ANSI_RESET);
                }
                printf("\n");
            }
        }

        if (e->num_parents > 0 && e->parent_indices[0] >= 0)
        {
            lanes[my_lane] = e->parent_indices[0];
        }
        else
        {
            for (int l = my_lane; l < lane_count - 1; l++)
                lanes[l] = lanes[l + 1];
            lane_count--;

            if (lane_count > 0 && my_lane < lane_count)
            {
                // join connector
                for (int l = 0; l < lane_count; l++)
                {
                    if (l == my_lane)
                        printf("%s|/%s", lane_color(l), ANSI_RESET);
                    else
                        printf("%s|%s ", lane_color(l), ANSI_RESET);
                }
                printf("\n");
            }
        }

        // deduplicating converged lanes
        for (int l = 0; l < lane_count; l++)
        {
            for (int m = l + 1; m < lane_count; m++)
            {
                if (lanes[l] == lanes[m])
                {
                    for (int k = m; k < lane_count - 1; k++)
                        lanes[k] = lanes[k + 1];
                    lane_count--;

                    for (int c = 0; c < lane_count; c++)
                    {
                        if (c == l)
                            printf("%s|/%s", lane_color(c), ANSI_RESET);
                        else
                            printf("%s|%s ", lane_color(c), ANSI_RESET);
                    }
                    printf("\n");
                    m--;
                }
            }
        }

        for (int p = 0; p < e->num_parents; p++)
        {
            int pi = e->parent_indices[p];
            if (pi >= 0)
            {
                indegree[pi]--;
                if (indegree[pi] == 0)
                    heap_push(&heap, pi, cg);
            }
        }
    }

    free(indegree);
    free(lanes);
    heap_free(&heap);
}
