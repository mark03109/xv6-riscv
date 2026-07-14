#include "kernel/types.h"
#include "user.h"

int
main(int argc, char* argv[]){
    int sleep_ticks = atoi(argv[1]);
    pause(sleep_ticks);
    exit(0);
}