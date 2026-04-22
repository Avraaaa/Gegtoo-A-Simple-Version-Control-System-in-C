#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "../../include/utils/lca.h"
#include "../../include/core/tree.h"
#include "../../include/core/object.h"
#include "../../include/utils/myers_diff.h"


//A dynamic array of unique commit ID strings 
typedef struct { 
    char **ids; 
    int count, cap; 
} IdSet;

//Queue for bfs
typedef struct { 
    char **data; 
    int head, tail, cap; 
} IdQueue;

typedef struct { 
    char *buf; 
    size_t len, cap; 
} StrBuf;

static void idset_init(IdSet *s){
    s->cap = 32;
    s->count = 0;
    s->ids = malloc(s->cap *sizeof(char*));
}

static int idset_has(const IdSet *s, const char *id){
    for(int i=0;i<s->count;i++){
        if(strcmp(s->ids[i],id)==0) return 1;
    }
    return 0;
}

static void idset_add(IdSet *s, const char *id){
    if(idset_has(s,id)) return;
    if(s->count>=s->cap){
        s->cap*=2;
        s->ids = realloc(s->ids,s->cap*sizeof(char *));
    }
    s->ids[s->count++] = strdup(id);
}

static void idset_free(IdSet *s) {
    for (int i = 0; i < s->count; i++) free(s->ids[i]);
    free(s->ids);
}

static void queue_init(IdQueue *q) {
    q->cap = 64; 
    q->head = q->tail = 0;
    q->data = malloc(q->cap * sizeof(char *));
}

static int queue_empty(const IdQueue *q) { 
    return q->head == q->tail; 
}

static void queue_push(IdQueue *q, const char *id) {
    if (q->tail >= q->cap) 
    { 
        q->cap *= 2; 
        q->data = realloc(q->data, q->cap * sizeof(char *)); 
    }
    q->data[q->tail++] = strdup(id);
}

static char *queue_pop(IdQueue *q) { 
    return q->data[q->head++]; 
}

static void queue_drain_free(IdQueue *q) {
    while (!queue_empty(q)) { 
        char *x = queue_pop(q); 
        free(x); 
    }
    free(q->data);
}


//Lowest common ancestor(bfs)
static IdSet collect_ancestors(const char *start){
    IdSet visited;
    idset_init(&visited);
    IdQueue q;
    queue_init(&q);
    queue_push(&q,start);

    while(!queue_empty(&q)){
        char *cur = queue_pop(&q);
        if(!idset_has(&visited, cur)){
            idset_add(&visited, cur);
            char parents[2][41];
            int pc = 0;
            read_commit_parents(cur,parents,&pc);
            for(int i=0;i<pc;i++){
                queue_push(&q,parents[i]);
            }
        }
        free(cur);
    }
    queue_drain_free(&q);
    return visited;
}

int find_merge_base(const char *commit_a, const char *commit_b, char base_out[41]){
    IdSet anc_a = collect_ancestors(commit_a);

    IdQueue q;
    queue_init(&q);
    queue_push(&q,commit_b);
    IdSet visited;
    idset_init(&visited);
    int found = 0;

    while(!queue_empty(&q) && !found ){
        char *cur = queue_pop(&q);
        if(!idset_has(&visited,cur)){
            idset_add(&visited,cur);
            if(idset_has(&anc_a,cur)){
                strncpy(base_out,cur,41);
                found = 1;
            }else{
                char parents[2][41];
                int pc = 0;
                read_commit_parents(cur,parents,&pc);
                for(int i=0;i<pc;i++){
                    queue_push(&q,parents[i]);
                }
            }
            
        }
        free(cur);
    }
    queue_drain_free(&q);
    idset_free(&visited);
    idset_free(&anc_a);
    if(found) return 0;
    else return 1;
}


//3-Way Line-Level Merge Helpers

static char **split_into_lines(const char *text, size_t len, int *count_out){
    if(len==0){
        *count_out = 0;
        return NULL;
    }
    int n = 1;
    for(size_t  i=0;i<len;i++){
        if(text[i]=='\n') n++;
    }
    char **lines = malloc(n*sizeof(char *));
    int idx = 0;
    size_t start = 0;
    for(size_t i=0;i<=len;i++){
        if(i==len || text[i]=='\n'){
            size_t l = i-start;
            lines[idx] = malloc(l+2);
            memcpy(lines[idx],text+start,l);
            if(i<len){
                lines[idx][l] = '\n';
                lines[idx][l+1] = '\0';
            }
            else{
                lines[idx][l] = '\0';
            }
            idx++;
            start = i+1;
        }
    }
    *count_out = idx;
    return lines;
}

static void free_lines(char **lines, int count){
    for(int i=0;i<count;i++){
        free(lines[i]);
    }
    free(lines);
}

static void sb_init(StrBuf *b) {
    b->cap = 256; b->len = 0;
    b->buf = malloc(b->cap);
    b->buf[0] = '\0';
}

static void sb_append(StrBuf *b, const char *s, size_t n) {
    while (b->len + n + 1 > b->cap) { 
        b->cap *= 2; 
        b->buf = realloc(b->buf, b->cap); 
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void sb_appends(StrBuf *b, const char *s) { 
    sb_append(b, s, strlen(s)); 
}

static int lines_equal(char **a, int na, char **b, int nb) {
    if (na != nb) return 0;
    for (int i = 0; i < na; i++) {
        if (strcmp(a[i], b[i]) != 0) {
            return 0;
        }
    }
    return 1;
}


static MergeStatus process_chunk(
    StrBuf *out,
    char **base_ch, int bn_ch,
    char **ours_ch, int on_ch,
    char **their_ch, int tn_ch)
{
    if (bn_ch == 0 && on_ch == 0 && tn_ch == 0) return MERGE_CLEAN;

    int ours_same   = lines_equal(ours_ch, on_ch,  base_ch, bn_ch);
    int theirs_same = lines_equal(their_ch, tn_ch, base_ch, bn_ch);
    int both_same   = lines_equal(ours_ch, on_ch,  their_ch, tn_ch);

    if (ours_same && theirs_same) {
        for (int i = 0; i < bn_ch; i++) sb_appends(out, base_ch[i]);
        return MERGE_CLEAN;
    }
    if (ours_same) {
        for (int i = 0; i < tn_ch; i++) sb_appends(out, their_ch[i]);
        return MERGE_CLEAN;
    }
    if (theirs_same) {
        for (int i = 0; i < on_ch; i++) sb_appends(out, ours_ch[i]);
        return MERGE_CLEAN;
    }
    if (both_same) {
        for (int i = 0; i < on_ch; i++) sb_appends(out, ours_ch[i]);
        return MERGE_CLEAN;
    }
    
    // Conflict markers
    sb_appends(out, "<<<<<<< HEAD\n");
    for (int i = 0; i < on_ch;  i++) {
        sb_appends(out, ours_ch[i]);
    }
    sb_appends(out, "=======\n");
    for (int i = 0; i < tn_ch;  i++) {
        sb_appends(out, their_ch[i]);
    }
    sb_appends(out, ">>>>>>> TARGET\n");
    return MERGE_CONFLICT;
}

//Core 3-Way Merge Operation


MergeStatus three_way_merge(
    const char *base,    size_t base_len,
    const char *ours,    size_t ours_len,
    const char *theirs,  size_t theirs_len,
    char      **result_out,
    size_t     *result_len_out)
{
    int bn = 0, on_ = 0, tn = 0;
    char **B = split_into_lines(base,   base_len,   &bn);
    char **O = split_into_lines(ours,   ours_len,   &on_);
    char **T = split_into_lines(theirs, theirs_len, &tn);

    int *match_o = malloc((bn + 1) * sizeof(int));
    int *match_t = malloc((bn + 1) * sizeof(int));
    
    //Using myers diff
    compute_myers_match(B, bn, O, on_, match_o);
    compute_myers_match(B, bn, T, tn,  match_t);

    StrBuf out; sb_init(&out);
    MergeStatus status = MERGE_CLEAN;
    int bi = 0, oi = 0, ti = 0;

    for (int k = 0; k < bn; k++) {
        // If a line exists in all three, it forms a "stable anchor"
        if (match_o[k] >= 0 && match_t[k] >= 0) {
            int next_oi = match_o[k];
            int next_ti = match_t[k];

            MergeStatus cs = process_chunk(&out,
                B + bi, k    - bi,
                O + oi, next_oi - oi,
                T + ti, next_ti - ti);
            
            if (cs != MERGE_CLEAN) status = MERGE_CONFLICT;

            sb_appends(&out, B[k]);
            bi = k + 1;
            oi = next_oi + 1;
            ti = next_ti + 1;
        }
    }
    
    MergeStatus cs = process_chunk(&out,
        B + bi, bn  - bi,
        O + oi, on_ - oi,
        T + ti, tn  - ti);
        
    if (cs != MERGE_CLEAN) status = MERGE_CONFLICT;

    *result_out      = out.buf;
    *result_len_out  = out.len;

    free(match_o); free(match_t);
    if (B) free_lines(B, bn);
    if (O) free_lines(O, on_);
    if (T) free_lines(T, tn);
    
    return status;
}