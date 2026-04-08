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

#include "../../include/utils/huffman.h"
#include "../../include/utils/myers_diff.h"
#include "../../include/core/fs.h"

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


