#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void memdump(char *fmt, char *data);

int
main(int argc, char *argv[])
{
  if(argc == 1){
    printf("Example 1:\n");
    int a[2] = { 61810, 2025 };
    memdump("ii", (char*) a);
    
    printf("Example 2:\n");
    memdump("S", "a string");
    
    printf("Example 3:\n");
    char *s = "another";
    memdump("s", (char *) &s);

    struct sss {
      char *ptr;
      int num1;
      short num2;
      char byte;
      char bytes[8];
    } example;
    
    example.ptr = "hello";
    example.num1 = 1819438967;
    example.num2 = 100;
    example.byte = 'z';
    strcpy(example.bytes, "xyzzy");
    
    printf("Example 4:\n");
    memdump("pihcS", (char*) &example);
    
    printf("Example 5:\n");
    memdump("sccccc", (char*) &example);
  } else if(argc == 2){
    // format in argv[1], up to 512 bytes of data from standard input.
    char data[512];
    int n = 0;
    memset(data, '\0', sizeof(data));
    while(n < sizeof(data)){
      int nn = read(0, data + n, sizeof(data) - n);
      if(nn <= 0)
        break;
      n += nn;
    }
    memdump(argv[1], data);
  } else {
    printf("Usage: memdump [format]\n");
    exit(1);
  }
  exit(0);
}

void
memdump(char *fmt, char *data)
{
  // Your code here.
  int i = 0;
  int j = 0;
  while(fmt[i]){
    if(fmt[i] == 'i'){
      int ret = 0;
      ret += data[j+3];
      for(int k = 2; k >= 0; k--){
        ret <<= 8;
        ret += data[j+k];
      }
      j+=4;
      printf("%d\n", ret);
    }
    else if(fmt[i] == 'p'){
      long long ret = 0;
      ret += (unsigned char)data[j+7];
      for(int k = 6; k >= 0; k--){
        ret <<= 8;
        ret += (unsigned char)data[j+k];
      }
      j+=8;
      printf("%x\n", (uint32)ret);
    }
    else if(fmt[i] == 'h'){
      short ret = 0;
      ret += data[j+1];
      for(int k = 0; k >= 0; k--){
        ret <<= 8;
        ret += data[j+k];
      }
      j+=2;
      printf("%d\n", ret);
    }
    else if(fmt[i] == 'c'){
      printf("%c\n", data[j]);
      j+=1;
    }
    else if(fmt[i] == 's'){
      char ret[9] = {'\0'};
      char *str = *(char **)(data+j);
      for(int k = 0 ; k < 8; k++){
        ret[k] = *str;
        str += sizeof(char);
      }
      printf("%s\n", ret);
      j+=8;
    }
    else if(fmt[i] == 'S'){
      int k = 0;
      while(data[j+(k++)] != '\0');
      printf("%s\n", &data[j]);
      j+=k;
    }
    i++;
  }
}
