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
#include "../../include/core/object.h"
#include "../../include/core/index.h"
#include "../../include/core/binary.h"
#include "../../include/utils/huffman.h"
#include "../../include/utils/sha1.h"

void database_store(Blob *blob)
{
    char header[64];
    int header_len = snprintf(header, sizeof(header), "%s %zu", blob->type, blob->size);

    size_t full_len = header_len + 1 + blob->size;
    unsigned char *full_data = malloc(full_len);
    if (!full_data)
    {
        perror("Malloc failed");
        return;
    }

    memcpy(full_data, header, header_len);
    full_data[header_len] = '\0';
    memcpy(full_data + header_len + 1, blob->data, blob->size);

    unsigned char hash_bin[20];
    sha1_hash(full_data, full_len, hash_bin);

    for (int i = 0; i < 20; i++)
    {
        sprintf(blob->id + (i * 2), "%02x", hash_bin[i]);
    }

    char dir_path[PATH_MAX];
    char final_path[PATH_MAX];
    char temp_path[PATH_MAX];

    snprintf(dir_path, sizeof(dir_path), ".geg/objects/%.2s", blob->id);
    snprintf(final_path, sizeof(final_path), "%s/%s", dir_path, blob->id + 2);
    snprintf(temp_path, sizeof(temp_path), ".geg/temp_%s", blob->id);

    FILE *tmp = fopen(temp_path, "wb");
    if (!tmp)
    {
        perror("Failed to create temp file");
        free(full_data);
        return;
    }

    fwrite(full_data, 1, full_len, tmp);
    fclose(tmp);

    create_directory(dir_path);
    compress(temp_path, final_path);
    remove(temp_path);
    free(full_data);
}


IndexEntry *store_and_index(const char *path, const char *content, size_t len)
{
    Blob b;
    b.data = (char *)content;
    b.size = len;
    strcpy(b.type, "blob");
    database_store(&b);

    write_file(path, content, len);

    struct stat st;
    if (stat(path, &st) != 0)
        return NULL;

    IndexEntry *ie = malloc(sizeof(IndexEntry));
    ie->ctime_sec = (uint32_t)st.st_ctime;
    ie->ctime_nsec = 0;
    ie->mtime_sec = (uint32_t)st.st_mtime;
    ie->mtime_nsec = 0;
    ie->dev = st.st_dev;
    ie->ino = st.st_ino;
    ie->mode = st.st_mode;
    ie->uid = st.st_uid;
    ie->gid = st.st_gid;
    ie->size = (uint32_t)len;
    hex_to_binary(b.id, ie->sha1);
    ie->path = strdup(path);
    ie->flags = strlen(path) & 0xFFF;

    return ie;
}


void restore_blob(const char *path, const char *id)
{

    if (strcmp(path, "geg") == 0 || strcmp(path, "./geg") == 0)
    {
        return;
    }

    char obj_path[PATH_MAX];
    char temp_path[PATH_MAX];
    snprintf(obj_path, sizeof(obj_path), ".geg/objects/%.2s/%s", id, id + 2);
    snprintf(temp_path, sizeof(temp_path), ".geg/temp_checkout_%s", id);

    if (access(obj_path, F_OK) == -1)
    {
        printf("Error: Blob %s missing.\n", id);
        return;
    }

    decompress(obj_path, temp_path);

    FILE *src = fopen(temp_path, "rb");
    if (!src)
        return;

    char type[10];
    size_t size;
    if (fscanf(src, "%s %zu", type, &size) != 2)
    {
        fclose(src);
        return;
    }
    fgetc(src);

    FILE *dst = fopen(path, "wb");
    if (dst)
    {
        char buffer[1024];
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0)
        {
            fwrite(buffer, 1, n, dst);
        }
        fclose(dst);
    }
    else
    {
        perror("Failed to write workspace file");
    }

    fclose(src);
    remove(temp_path);
}


void restore_tree(const char *tree_id, const char *base_path)
{
    char obj_path[PATH_MAX];
    char temp_path[PATH_MAX];
    snprintf(obj_path, sizeof(obj_path), ".geg/objects/%.2s/%s", tree_id, tree_id + 2);
    snprintf(temp_path, sizeof(temp_path), ".geg/temp_tree_%s", tree_id);

    if (access(obj_path, F_OK) == -1)
        return;

    decompress(obj_path, temp_path);
    FILE *fp = fopen(temp_path, "rb");
    if (!fp)
        return;

    char type[10];
    size_t size;
    if (fscanf(fp, "%s %zu", type, &size) != 2 || strcmp(type, "tree") != 0)
    {
        fclose(fp);
        return;
    }
    fgetc(fp);

    long data_start = ftell(fp);
    while (ftell(fp) < data_start + size)
    {
        char mode[10];
        char name[255];
        unsigned char bin_sha[20];
        char hex_sha[41];

        if (fscanf(fp, "%s", mode) != 1)
            break;
        fgetc(fp);

        int i = 0;
        char c;
        while ((c = fgetc(fp)) != '\0' && c != EOF && i < 254)
            name[i++] = c;
        name[i] = '\0';

        if (fread(bin_sha, 1, 20, fp) != 20)
            break;
        for (int k = 0; k < 20; k++)
            sprintf(hex_sha + (k * 2), "%02x", bin_sha[k]);

        char full_path[PATH_MAX];
        if (base_path && strlen(base_path) > 0)
            snprintf(full_path, sizeof(full_path), "%s/%s", base_path, name);
        else
            snprintf(full_path, sizeof(full_path), "%s", name);

        if (strcmp(mode, "100644") == 0)
        {
            restore_blob(full_path, hex_sha);
        }
        else
        {
            mkdir(full_path, 0755);
            restore_tree(hex_sha, full_path);
        }
    }
    fclose(fp);
    remove(temp_path);
}


char *geg_blob_content(const char *blob_id, size_t *size_out){
    char obj_path[PATH_MAX], temp_path[PATH_MAX];
    snprintf(obj_path,sizeof(obj_path),".geg/objects/%.2s/%s",blob_id,blob_id+2);
    snprintf(temp_path,sizeof(temp_path),".geg/temp_blob_%s",blob_id);

    *size_out = 0;
    if(access(obj_path,F_OK)==-1) return NULL;

    decompress(obj_path,temp_path);
    FILE *fp = fopen(temp_path,"rb");
    if(!fp) return NULL;

    char type[10];
    size_t size;
    if(fscanf(fp,"%s %zu",type,&size)!=2){
        fclose(fp);
        remove(temp_path);
        return NULL;
    }
    fgetc(fp);//for consuming the null byte

    char *content = malloc(size+1);
    if(size>0) fread(content,1,size,fp);
    content[size] = '\0';//Null terminator

    fclose(fp);
    remove(temp_path);

    *size_out = size;
    return content;
}