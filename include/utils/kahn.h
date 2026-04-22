#ifndef UTILS_KAHN_H
#define UTILS_KAHN_H

#include "../../include/core/commit_graph.h"

typedef void (*CommitPrinter)(const char *oid, const char *graph_prefix);

void kahn_log(const CommitGraph *cg, CommitPrinter printer);

#endif
