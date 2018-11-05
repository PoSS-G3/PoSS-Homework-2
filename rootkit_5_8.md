# 隐藏特定后缀名的文件
1. 原理（步骤）
	1. hook `sys_getdents` 系统调用的返回值。

		`int sys_getdents(unsigned int fd, struct dirent *dirp,unsigned int count) `

		其中 `fd` 为指向目录文件的文件描述符，该函数根据 `fd` 所指向的目录文件读取相应 `dirent` 结构，并放入 `dirp` 中，其中 `count` 为 `dirp` 中返回的数据量，正确时该函数返回值为填充到 `dirp` 的字节数。

	2. 遍历 `sys_getdents` 的返回值中包含的文件，对比这些文件的文件名是否含有我们需要隐藏的后缀名。

		其中，`check_file_suffix()` 函数检查文件后缀名
	
		`Should_be_hidden()` 函数判断文件是否需要隐藏

	3. 如果需要隐藏，修改返回值中的相应内容。

	功能：隐藏包含特定后缀的文件，在 `sys_getdents` 中过滤特定后缀的目录项

2. 隐藏文件的相关函数

	`check_file_suffix`		—— 检查文件后缀名

	`Should_be_hidden()` 	—— 判断遍历到的sys_getdents系统调用的返回值中的文件是否需要隐藏

	`new_sys_getdents` —— 对 `getdents` 的 `hook` 函数，返回目录文件的相应内容

3. [代码注释](https://github.com/PoSS-G3/Hymmnos-rootkit/blob/master/hymmnos.c "代码注释")

4. 测试

	我们隐藏的是后缀名为 `reyvateil` 的文件
	
	首先，在 `/Desktop/newworld` 目录下创建一个新的文件 `1.reyvateil`
![](/screenshot/rootkit58-1.png)
 
	查看后缀名为 `reyvateil` 的文件，返回结果为 `1.reyvateil`；

	加载 `hymmnos` 模块之后，`1.reyvateil` 文件被隐藏；
	
	卸载 `hymmnos` 模块之后，隐藏的文件 `1.reyvateil` 被显示

# 控制（阻止）新的内核模块加载
1. 原理

	当攻击者获得 root 权限后，首先应该做的不是提供后门，而是堵上漏洞，以及控制好其他程序，以防其加载其他反 rootkit 模块与我们在内核态血拼。

	控制内核模块的加载，从通知链机制下手。当某个子系统或者模块发生某个事件时，该子系统主动遍历某个链表，而这个链表中记录着其他子系统或者模块注册的事件处理函数，通过传递恰当的参数调用这个处理函数达到事件通知的目的。

	注册一个模块通知处理函数，在模块完成加载之后、开始初始化之前， 即模块状态为 `MODULE_STATE_COMING`，将其初始函数掉包成一个什么也不做的函数，就会导致模块不能完成初始化。

	内核模块的加载过程，我们注册的通知链处理函数是在 `notifier_call_chain` 函数里被调用的，调用层次为：
	`blocking_notifier_call_chain ->__blocking_notifier_call_chain -> notifier_call_chain。`

	如何注册模块通知处理函数:用于描述通知处理函数的结构体是 `struct notifier_bloc`k 。注册或者注销模块通知处理函数可以使用 `register_module_notifier` 或者 `unregister_module_notifier`，编写一个通知处理函数，然后填充 `struct notifier_block` 结构体，最后使用 `register_module_notifier` 注册。

	功能：阻止新模块的加载

2. 相关函数

	`fake_init，fake_exit`
	
	我们自己写的 `init`，`exit` 函数，替换其他模块的 `init` 和 `exit`

	`module_notifier`
	
	监听新模块 `regist_komon`，`unregist_komon`
	
	注册与解除注册模块 `notifier` 、

3. [代码注释](https://github.com/PoSS-G3/Hymmnos-rootkit/blob/master/hymmnos.c "代码注释")

4. 测试

	`module_notifier` 函数是用来监听新模块的；测试流程：

	1. 先把当前模块 `hymmnos` 加载到内核，此时会将真实的 `init()` 和 `exit()` 替换为 `fake_init()` 和 `fake_exit()` ；由于将初始化函数替换，导致新模块不能加载。
	2. 加载一个新的模块；加载之后 `dmesg` 可以看到 `fake_init()` 的 `printk` 内容；
	3. 卸载这个简单的模块，能从 `dmesg` 中看到 `fake_exit()` 的 `printk` 内容；
	4. 如果成功看到这两个 `printk` 的信息，则监听成功

