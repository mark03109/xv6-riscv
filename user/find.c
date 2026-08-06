#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

void find(char* path, char* name){
    int fd;
    struct stat st;
    struct stat sub_st;
    struct dirent de;
    char buf[512], *p;
    
    if((fd = open(path, O_RDONLY)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    if(st.type != T_DIR){
        fprintf(2, "find: %s is not a directory\n", path);
    }
    strcpy(buf, path);
    p = buf + strlen(path);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
        if(de.inum == 0)continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if(stat(buf, &sub_st) < 0){
            printf("find: cannot stat %s\n", buf);
            continue;
        }
        if(sub_st.type == T_DIR){
            if(!strcmp(".", de.name) || !strcmp("..", de.name))continue;
            find(buf, name);
        }
        else{
            if(!strcmp(de.name, name)){
                printf("%s/%s\n", path, name);
            }
        }
    }   
    close(fd);
}

int 
main(int argc, char* argv[]){
    if(argc < 3){
        printf("find: no enough arguments\n");
    }
    else{
        find(argv[1], argv[2]);
    }
    exit(0);
}