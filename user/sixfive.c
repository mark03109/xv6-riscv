#include "kernel/types.h"
#include "user.h"
#include "kernel/fcntl.h"

int toInt(char* digit, int end){
    int ret = 0;
    int start = 0;
    for(;start < end; start++){
        ret = ret*10 + (digit[start] - '0');
    }
    return ret;
}

void
process(int fd){
    const char seperators[8] = {'-','\r','\t','\n','.','/',',','\0'};
    char digit[512] = {'\0'};
    char buf[1];
    char seperator_flag = 0x01;

    for(;;){
        int n = read(fd, buf, sizeof(buf));
        if(n < 0){
            fprintf(2,"read error\n");
        }
        else if(n == 0){
            break;
        }
        else{
            int count = 0;
            if(strchr(seperators, buf[0])){
                count = 0;
            }
            else if(buf[0] >= '0' && buf[0] <= '9' && seperator_flag){
                digit[count++] = buf[0];
            }
            else{
                continue;
            }

            while(read(fd, buf, sizeof(buf))){
                if(buf[0] >= '0' && buf[0] <= '9'){
                    digit[count++] = buf[0];
                }
                else if(strchr(seperators, buf[0])){
                    seperator_flag = 1;
                    break;
                }
                else{
                    seperator_flag = 0;
                    break;
                }
            }

            if(seperator_flag && count){
                int temp = toInt(digit, count);
                if(temp % 5 == 0 || temp % 6 == 0){
                    fprintf(1, "%d\n", temp);
                }
            }
        }
    }
}

int
main(int argc, char* argv[]){
    if(argc < 2){
        fprintf(2, "Usage: sixfive file...\n");
        exit(1);
    }

    for(int i = 1; i < argc; i++){
        int fd = open(argv[i], O_RDONLY);
        if(fd < 0){
            fprintf(2, "sixfive: cannot open %s\n", argv[i]);
            continue;
        }
        process(fd);
        close(fd);
    }
    exit(0);
}
