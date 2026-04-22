#ifndef CORE_COMMIT_GRAPH_H
#define CORE_COMMIT_GRAPH_H

#include <stdint.h>

#define CG_MAX_PARENTS 2

typedef struct {
    char     oid[41];
    int      parent_indices[CG_MAX_PARENTS]; // -1 meaning no parent
    int      num_parents;
    uint32_t generation; // 0 for roots, 1+max(parent gens) otherwise
} CGraphEntry;

typedef struct {
    CGraphEntry *entries;
    int          count;
    int          capacity;
} CommitGraph;

void cg_build(CommitGraph *cg);
void cg_free(CommitGraph *cg);
int  cg_find(const CommitGraph *cg, const char *oid);

#endif
