# Tinyhttp源码分析

* 简介

Tinyhttp是一个轻量型型Http Server, 使用C语言开发，全部代码500多行。

* 源码剖析

具体逻辑：主程序无限循环，监听套接字上是否有连接请求。一个线程处理一个连接请求，并解析HTTP请求，做一些判断。如果请求的是静态文件就输出给客服端。如果请求的是动态文件，就创建一个进程执行cgi程序，并创建管道进行父子进程间通信。

![TinyHttpd流程图](https://raw.githubusercontent.com/tuhbo/TinyHttpd/master/images/TinyHttpd%E6%B5%81%E7%A8%8B%E5%9B%BE.jpg)

**每个函数的作用：**

```c
// accept_request函数：处理从套接字上监听到的一个HTTP请求，此函数很大部分体现服务器处理请求流程。
void accept_request(void *);
// bad_request函数：返回给客户端这是个错误请求，HTTP状态码400 Bad Request。
void bad_request(int);
// cat函数：读取服务器上某个文件写到socket套接字。
void cat(int, FILE *);
// cannot_execute函数：处理发生在执行cgi程序时出现的错误。
void cannot_execute(int);
// error_die函数：把错误信息写到perror并退出。
void error_die(const char *);
// execute_cgi函数：运行cgi程序的处理，是主要的函数。
void execute_cgi(int, const char *, const char *, const char *);
// get_line函数：读取套接字的一行，把回车换行等情况都统一为换行符结束。
int get_line(int, char *, int);
// headers函数：把HTTP响应的头部写到套接字。
void headers(int, const char *);
// not_found函数：处理找不到请求的文件时的情况。
void not_found(int);
// serve_file函数：调用cat函数把服务器文件返回给浏览器
void serve_file(int, const char *);
// startup函数：初始化httpd服务，包括建立套接字，绑定端口，进行监听等。
int startup(u_short *);
// unimplemented函数：返回给浏览器表明收到的HTTP请求所用的method不被支持。
void unimplemented(int);
```

**建议按照：main()->startup()->accept_request()->execute_cgi()的流程阅读源码**

