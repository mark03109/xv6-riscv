#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int 
main(int argc, char *argv[]){
    unsigned int i = 0x00646c72;
	printf("H%x Wo%s\n", 57616, (char *) &i);

    char a = 'o';
    char b = 'w';

    printf("%s\n", &a);
    printf("%s\n", &b);

    char buf[20];
    for(int i = 0; i < 20; i++){
        printf("%p\n", &buf[i]);
    }
    char*p = &a + 1;
    for(int i = 0; i < 6; i++){
        printf("%p: %c\n",p,*p);
        p++;
    }
    printf("%p\n", &b);
    printf("%p\n", &a);
    printf("%p\n\n", &i);

    printf("x=%d y=%d\n", 3);
    exit(0);
}