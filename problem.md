# Lab1

## Boot xv6 (easy)

vscode直接打开wsl的文件夹，会遇到windows环境和linux环境冲突。
- vscode中安装WSL插件，远程连接。这样vscode的git，终端服务都是使用的wsl环境下的

在xv6的shell里输入Ctrl+P命令会被vscode快捷键劫持
- vscode的用户setting里禁用此快捷键

## sixfive (moderate)

如何在xv6中使用gdb进行调试
- 一个终端输入make qemu-gdb，另一个终端输入gdb kernel/kernel，输入target remote localhost:26000
- 终端2输入Ctrl+C中断，进入(gdb)，输入file user/_sixfive，输入b main，输入c继续运行
- 终端1执行命令sixfive sixfive.txt, 终端2会在main函数处中断，输入n逐行调试，输入c继续运行
- n 下一行不进入函数；s 下一行，若有函数则进入；l 查看当前代码；p 查看变量值；where 查看当前执行的位置；i b 查看断点设置；d 删除断点；x/s x查看内存，/s按字符串格式显示

## find(moderate)

如何理解xv6的cwd
- shell本身作为用户进程，就有自己的目录项，当你在其中输入一个程序执行时，会fork进程，继承shell此时的目录项

## exec(moderate)
如何理解xv6的exec
- `exec(char*, char**)`函数第一个参数传入程序的名称，第二个参数传入参数数组(注意第一个参数是程序名称，不要省略）
- exec前需要fork新进程执行