#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

#define DATASIZE (8*4096)

int
main(int argc, char *argv[])
{
  // Your code here.
  char* atk = sbrk(DATASIZE);
  int i = 0;
  for(; i < DATASIZE; i++){
    if(strcmp(&atk[i], "This may help.") == 0){
      printf("%s\n", &atk[i+16]);
      exit(0);
    }
  }
  exit(1);
}
