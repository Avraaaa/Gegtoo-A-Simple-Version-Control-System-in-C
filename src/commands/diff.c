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

#include "../../include/core/fs.h"
#include "../../include/core/index.h"
#include "../../include/utils/myers_diff.h"
#include "../../include/utils/sha1.h"
#include "../../include/commands.h"
#include "../../include/core/tree.h"

void geg_diff(int argc, char *argv[])
{
    int syntax_mode = 0;
    for (int i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], "--syntax") == 0 || strcmp(argv[i], "--word-diff") == 0)
            syntax_mode = 1;
    }

    GegIndex *index = load_index();
    if (!index)
        return;

    for (uint32_t i = 0; i < index->count; i++)
    {
        IndexEntry *entry = index->entries[i];

        int should_process = 0, has_file_args = 0;
        for (int a = 2; a < argc; a++)
        {
            if (argv[a][0] != '-')
            {
                has_file_args = 1;
                break;
            }
        }

        if (!has_file_args)
        should_process = 1;

        else
        {
            for (int arg_idx = 2; arg_idx < argc; arg_idx++)
            {
                if (argv[arg_idx][0] == '-')
                    continue;
                if (strcmp(entry->path, argv[arg_idx]) == 0)
                {
                    should_process = 1;
                    break;
                }
            }
        }

        if (should_process == 0)
        continue;


        struct stat st;
        if (stat(entry->path, &st) == 0)
        {
            int modified = 0;

            if (entry->size != st.st_size)
        modified = 1;

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
        modified = 1;

                }
            }

            

            if (modified)
        diff_file(entry->path, entry->sha1, syntax_mode);

        }
    }

    free_index(index);
}
