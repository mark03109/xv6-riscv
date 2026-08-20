#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int 
main(int argc, char *argv[]){
    unsigned int i = 0x00646c72;
	printf("H%x Wo%s\n", 57616, (char *) &i);
    
    exit(0);
}