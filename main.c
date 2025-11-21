#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>




void create_directory(const char *path){
    struct stat st = {0};
    if(stat(path,&st)==-1){
        if(mkdir(path,0755)!=0){
            perror("mkdir");
        }
    }
}


void geg_init(){

    create_directory(".geg");
    create_directory(".geg/objects");
    create_directory(".geg/refs");
    create_directory(".geg/refs/heads");

    FILE *head_file = fopen(".geg/HEAD","w");
    if(head_file){
        fprintf(head_file,"ref: refs/heads/master");
        fclose(head_file);
    }

    FILE *master_branch_file = fopen(".geg/refs/heads/master","w");
    if(master_branch_file) fclose(master_branch_file);

    FILE *index_file = fopen(".geg/index","w");
    if(index_file) fclose(index_file);

    printf("Initialized an empty geg repository in .svcs/\n");

}


int main(int argc, char *argv[]){
    
    if(argc<2){
        printf("Usage: ./geg <command> [<args>]\n");
    }

    const char *command = argv[1];

    if(strcmp(command,"init")==0){
        geg_init();
    }

    return 0;

}