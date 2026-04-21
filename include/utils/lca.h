#ifndef UTILS_LCA_H
#define UTILS_LCA_H

#include <stddef.h>

typedef enum {
    MERGE_CLEAN    = 0,
    MERGE_CONFLICT = 1
} MergeStatus;

int find_merge_base(const char *commit_a, const char *commit_b, char base_out[41]);

MergeStatus three_way_merge(
    const char *base,    size_t base_len,
    const char *ours,    size_t ours_len,
    const char *theirs,  size_t theirs_len,
    char      **result_out,
    size_t     *result_len_out);

#endif