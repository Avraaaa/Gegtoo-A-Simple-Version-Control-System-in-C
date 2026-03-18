#include "../../include/core.h"
#include "../../include/commands.h"

void geg_add(int argc, char *argv[])
{

    if (argc < 3)
    {
        printf("Usage: ./geg add <files...>\n");
        return;
    }

    char **all_files = NULL;
    int total_new_files = 0;

    for(int i=2;i<argc;i++){

        struct stat st;
        if(stat(argv[i],&st)==0){

            //passing '.' as base and argv[i] as relative returns paths like folder/file.c
            if(S_ISDIR(st.st_mode)){
                explore_directory(".",argv[i],&all_files,&total_new_files);
            }
            else if(S_ISREG(st.st_mode)){
                //add directory to list, since its a regular file
                all_files = realloc(all_files,sizeof(char *)*(total_new_files+1));
                all_files[total_new_files] = strdup(argv[i]);
                total_new_files++;
            }
        }
        else{
            printf("Warning: Could not read '%s'\n", argv[i]);   
        }

    }

    if(total_new_files == 0){
        printf("Nothing to add.\n");
        return;
    }

    GegIndex *old_index = load_index();
    uint32_t old_count = 0;

    if (old_index)
    {
        old_count = old_index->count;
    }

    int max_files = old_count + total_new_files;
    IndexEntry **entries = malloc(sizeof(IndexEntry *) * max_files);
    int count = 0;

    if (old_index)
    {
        for (uint32_t i = 0; i < old_count; i++)
        {
            IndexEntry *old_entry = old_index->entries[i];
            int is_being_updated = 0;
            // Check if the any of the old files or new files match
            for (int j = 0; j < total_new_files; j++)
            {
                if (strcmp(old_entry->path,all_files[j]) == 0){
                    is_being_updated = 1;
                    break;
                }
            }
            //check if any old file was updated
            if(is_being_updated){
                free(old_entry->path);
                free(old_entry);
            }

            else{
                entries[count++] = old_entry;
            }
        }
        free(old_index->entries);
        free(old_index);
    }

    for (int i = 0; i < total_new_files; i++)
    {

        size_t size;
        char *content = read_workspace_files(all_files[i], &size);

        if (content)
        {

            Blob blob;
            blob.data = content;
            blob.size = size;
            strcpy(blob.type, "blob");
            database_store(&blob);

            struct stat st;
            stat(all_files[i], &st);

            IndexEntry *entry = malloc(sizeof(IndexEntry));
            entry->ctime_sec = st.st_ctime; // Permission changed time
            entry->ctime_nsec = 0;
            entry->mtime_sec = st.st_mtime; // Modified time
            entry->mtime_nsec = 0;
            entry->ino = st.st_ino; // Inode -  A number tht is unique for every file within a specific linux system
            entry->mode = st.st_mode;
            entry->dev = st.st_dev; // Returns the device ID
            entry->uid = st.st_uid; // User ID
            entry->gid = st.st_gid; // Group ID
            entry->size = size;
            hex_to_binary(blob.id, entry->sha1);
            entry->path = strdup(all_files[i]);
            entry->flags = strlen(all_files[i]) & 0xFFF;

            entries[count++] = entry;
            free(content);
        }
        free(all_files[i]);
    }
    free(all_files);

    qsort(entries, count, sizeof(IndexEntry *), compare_Index_Entries_by_path);

    FILE *index_file = fopen(".geg/index", "wb");

    if (!index_file)
    {
        perror("fopen");
        return;
    }

    size_t capacity = 4096;
    unsigned char *full_data = malloc(capacity);
    size_t offset = 0;

    unsigned char header[12];
    memcpy(header, "DIRC", 4);
    pack32_be(2, header + 4);
    pack32_be(count, header + 8);

    memcpy(full_data + offset, header, 12);
    offset += 12;

    for (int i = 0; i < count; i++)
    {
        IndexEntry *e = entries[i];
        unsigned char entry_buf[62];
        pack32_be(e->ctime_sec, entry_buf);
        pack32_be(e->ctime_nsec, entry_buf + 4);
        pack32_be(e->mtime_sec, entry_buf + 8);
        pack32_be(e->mtime_nsec, entry_buf + 12);
        pack32_be(e->dev, entry_buf + 16);
        pack32_be(e->ino, entry_buf + 20);
        pack32_be(e->mode, entry_buf + 24);
        pack32_be(e->uid, entry_buf + 28);
        pack32_be(e->gid, entry_buf + 32);
        pack32_be(e->size, entry_buf + 36);
        memcpy(entry_buf + 40, e->sha1, 20);
        pack16_be(e->flags, entry_buf + 60);

        unsigned int name_len = strlen(e->path);

        if (offset + 62 + name_len + 8 + 20 > capacity)
        {
            capacity *= 2;
            full_data = realloc(full_data, capacity);
        }

        memcpy(full_data + offset, entry_buf, 62);
        offset += 62;
        memcpy(full_data + offset, e->path, name_len);
        offset += name_len;

        int padding = 8 - ((62 + name_len) % 8);
        memset(full_data + offset, 0, padding);
        offset += padding;

        free(e->path);
        free(e);
    }

    free(entries);

    // For writing the checksum
    unsigned char index_sha[20];
    sha1_hash(full_data, offset, index_sha);
    memcpy(full_data + offset, index_sha, 20);
    offset += 20;

    fwrite(full_data, 1, offset, index_file);
    fclose(index_file);
    free(full_data);
}
