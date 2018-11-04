| 函数 | 功能 |
|`static void extract_type_1_socket_inode(const char lname[], long *inode_p)`|从类似于socket:[12345]中提取12345，该数字即是inode节点，否则返回-1|
|`int load_inodes_of_process(const char *name)`|检查需要被隐藏进程的fd，如果fd中存在软连接到socket的，软链接的目标将是类似socket:[12345]的形式，12345即是socket的inode节点，将这些inode节点记录下来|
|`void load_inodes_to_hide(void)`|从需要被隐藏的进程中寻找需要被隐藏的socket结点|
|`char *next_column(char *ptr)`|读取下一行，帮助函数|
|`int new_seq_show(struct seq_file *seq, void *v)`|对/proc/net/tcp文件对show函数对hook函数，该文件是特殊文件，通过他可以获得tcp连接信息。调用原始函数后，对内容进行过滤，删除掉需要被隐藏的inode条目|
