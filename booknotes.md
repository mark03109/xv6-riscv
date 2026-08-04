# Notes of "xv6: a simple, Unix-like teaching operating system"

## Chapter1:

### Processes and memory

System call `fork()` is to create a child process, who copis the memoty of its father process;
In father process, `fork()` returns its child's PID; In child's process, it returns 0;

System call `exit(int)` is for a child process to stop executing and release resources. `int` 0 indicates success and 1 indicates falure;
System call `wait(int *)` is for a father process to be awaked by a exited or killed child process. Argument `int*` is to carry out the exit status of the child process

System call `exec(const char*, char**)` is to load a new program into this process, which means the instruction, data, stack parts in the main mememoty is totally replaced.
`const char* ` is the file name(pwd) of the new program, and the `char**` is passed to the `main(int argc, char* argv[])`of the new program.
Attetion that the rest part of the replaced process will not be executed if the function `exec()` succeeded

### I/O and File description 

File descriptor is the index of a particular file.
In Unix/Linux, we can't just `write("hello.txt", "abc", 3)`; Instead, we need first do `int fd = open("hello.txt", O_WRONLY);`, then `write(fd, "hello", 5);`.
When each process starts, it opens 3 file descriptor by default. `fd = 0/1/2`, indicates "stdin, stdout, stderr".

System calls `int write(int fd, char* buf, int n)` and `int read(int fd, char* buf, int n)` can be now easily understood; They return the number of bytes that are successfully read or written.

Example: Program cat 
```cpp
char buf[512];

int n;
for(;;){
    n = read(0, buf, sizeof buf);
    if(n == 0)
        break;
    if(n < 0){
        fprintf(2, "read error\n");
        exit(1);
    }
    if(write(1, buf, n) != n){
        fprintf(2, "write error\n");
        exit(1);
    }
}
```

Example: cat < input.txt
```c
char *argv[2];
argv[0] = "cat";
argv[1] = 0;
if(fork() == 0) {
    close(0);
    open("input.txt", O_RDONLY);
    exec("cat", argv);
}
```
How elegant! 
Process first close standin, so the file input.txt will have its fd = 0; Then cat can achieve its function by reading fd = 0

### Pipes

System call `pipe(int* p)` creates a circle buffer, and carrys out two file diescriptor, `p[0]` for read and `p[1]` for write.

### File System

System call `chadir(const char*)` changes the current directory to `char*`

System call `mknod(const char* pos, int major, int minor)` creates a file refers to a device; `pos` indicates its location and name, major/minor defines the which device is refered.

The file name isn't the file itself
The same underlying file, called an inode, can have multiple names, called links. Each link of the same file contains a file name and a reference to an inode.
System call `fstat(int fd, struct stat* stat)` retrieves information from the inode that a fd refers to and save the information to stat.
`struct stat` defined as:

```c
#define T_DIR  1 // Directory
#define T_FILE 2 // File
#define T_DEVICE 3 // Device
struct stat {
    int dev;  // File system’s disk device
    uint ino; // Inode number
    short type; // Type of file
    short nlink; // Number of links to file
    uint64 size; // Size of file in bytes
};
```

System call `link(const char* a, const char* b)` creates another name of a file(or an inode);

