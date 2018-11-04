# 监视特定网络包，开启后门（功能9）

## 模块功能原理
这一部分的功能首先rootkit需要hook上netfilter协议栈，监听7777端口的网络数据包，当收到协议类型为icmp echo的数据包，同时检查数据包头中是否含有字符串“tonelico”，若有，则认定为需要攻击者需要开启后门，返回一个shell给攻击者。

## 核心函数
1. 注册钩子函数并且判断指令数据包

	`magic_packet_hook(const struct nf_hook_ops *ops, struct sk_buff *socket_buffer, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))`

	该函数在 netfilter 协议栈上注册了钩子，监听网络数据包。对于进入的数据包，首先检测是否为icmp协议，，检查 icmp 头和数据段是否不为空，检查icmp协议的类型是否为 `echo` ，若不满足以上条件，则不是 rootkit 需要的指令包，将包继续向上发出。继续检查，取出数据包中包头数据段开头的内容，是否为字符串 “tonelico” ，若是则认定为该数据包为攻击者发出的指令包，需要 rootkit 开启后门反弹 shell，在函数内进一步调用 `decode_n_spawn` 函数。

2. 解析网络数据包

	`decode_n_spawn(const char *data)`

	在 `magic_packet_hook` 函数中实际上是先将硬编码的 TOKEN 值进行异或以后直接和数据包中的数据进行比较，因此在验证成功以后还需要进一步解析数据。首先对数据段进行异或获取数据明文，按照约定的格式，以空格为分隔符获取ip和端口号，调用 `shell_exec_queue` 函数反弹一个 shell。

3. 反弹shell

    	shell_exec_queue(char *path, char *ip, char *port)
    	shell_execer(struct work_struct *work)
    	exec(char **argv)

	这三个函数的功能为反弹一个 shell。首先接收参数，第一个参数为 shell 的地址，第二、三个参数分别为 ip 地址及端口号。创建一个系统任务，优先级为GFP_KERNEL级别，初始化任务，将 shell 的地址、ip、端口传给任务，在 shell_execer 中使用函数 `call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC)` 创建一个进程，这个函数能够异常方便地在内核中直接新建和运行用户空间程序，并且该程序具有 root 权限。其中第一个参数为要执行shell的路径，第二个为程序参数，第三个为环境变量参数，第四个 `UMH_WAIT_PROC` 使得其可以直接在中断的上下文中使用。

## 其他相关函数
1. `int atoi(char *str)`

	将输入的字符串转化为数字，返回一个整型。

2. `void s_xor(char *arg, int key, int nbytes)`

	将输入的字符串arg按位与输入的key进行异或。

3. `void regist_backdoor(void)`

	实现后门注册功能，将后门标志位更改为1，则调用magic_packet_hook函数注册钩子函数。

4. `void unregist_backdoor(void)`

	实现后门注销功能，将后门标志位更改为0，则同时取消对netfilter的hook。

## 数据包构造文件

攻击者需要构造指令数据包，具体的代码注释见文件 `send_cmd.c`。

# 将当前进程权限改为root（功能10）

分两种情况实现 rootkit 的提权。实现将 rootkit 权限更改为 root 级别的关键是将进程的 uid、gid 等设置为0。

当内核版本小于 2.6.30 时，手动设置当前进程的 uid、suid、euid、gid、egid、fsuid、fsgid 为0，再使用 `cap_set_full` 函数使当前进程获得最大的权能。

内核版本 2.6.30 之后弃用了 `cap_set_full` 函数，直接调用 `commit_creds` 函数就可以获得 root 权限。
