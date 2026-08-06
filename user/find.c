#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

void find(char* path, char* name, int exec_argc, char* exec_argv[]){
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
            find(buf, name, exec_argc, exec_argv);
        }
        else{
            if(!strcmp(de.name, name)){
                if(exec_argc){
                    if(fork() == 0){
                        char *new_argv[32];
                        for(int i=0;i<exec_argc;i++)
                            new_argv[i]=exec_argv[i];
                        new_argv[exec_argc]=buf;
                        new_argv[exec_argc+1]=0;
                        exec(new_argv[0], new_argv);
                        exit(0);
                    }
                    else{
                        wait(0);
                    }
                }
                else{
                    printf("%s/%s\n", path, name);
                }
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
    else if(argc == 3){
        find(argv[1], argv[2], 0, argv+3);
    }
    else{
        if(!strcmp("-exec", argv[3])){
            find(argv[1], argv[2], argc-4, argv+4);
        }
    }
    exit(0);
}