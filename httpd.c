#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

//处理从套接字上监听到的一个HTTP请求
void accept_request(void *arg);
//错误请求
void bad_request(int);
//读取文件
void cat(int, FILE *);
//无法执行
void cannot_execute(int);
//错误输出
void error_die(const char *);
//执行cgi脚本
void execute_cgi(int, const char *, const char *, const char *);
//读取一行数据
int get_line(int , char *, int);
//返回httpd报文头
void headers(int, const char *);
//没有发现文件
void not_found(int);
//如果不是CGI文件，直接读取文件返回给请求的http客户端
void serve_file(int, const char *);
//初始化httpd服务，包括建立套接字，绑定端口，进行监听
int startup(u_short *);
//返回给浏览器表明收到的HTTP请求所用的method不被支持
void unimplemented(int);

/* Print out an error message with perror()
 *
 */

 void error_die(const char *message)
 {
     perror(message);
     exit(1);
 }

/* 初始化http服务器，监听端口也可以随机分配一个端口
 */
int startup(u_short *port)
{
    int httpd = 0;
    /*创建一个socket*/
    httpd = socket(AF_INET, SOCK_STREAM, 0);
    if(httpd == -1)
        error_die("socket");
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(*port);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    /*将socket绑定到对应的端口上*/
    if(bind(httpd, (struct sockaddr *)&servaddr,sizeof(servaddr))<0)
        error_die("bind");
    /*如果当前指定的端口号为0，则动态随机分配一个端口*/
    if(*port == 0)
    {
        socklen_t servaddr_len = sizeof(servaddr);
        if(getsockname(httpd, (struct sockaddr *)&servaddr, &servaddr_len)==-1)
            error_die("getsockname");
        *port = ntohs(servaddr.sin_port);

    }
    /*监听绑定的端口*/
    if(listen(httpd, 5)<0)
        error_die("listen");
    return httpd;
}

/*如果资源没有找到得返回给客户端下面的信息*/
void not_found(int client)
{
    char buf[1024];
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}
/*如果对应的请求方法没有实现，就返回此信息*/
void unimplemented(int client)
{
    char buf[1024];
    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}
/*读取一行。只要发现c为\n,就认为是一行结束。如果读到'\r'时，使用MSG_PEEK的方式偷取
 *socket接收缓冲区中的一个字符，但这个字符还存在接收缓冲区。如果是下一个字符则不处理，将
 *c置为\n。
 */

int get_line(int sock, char *buf, int size)
{
    int i=0;
    char c = '\0';
    int n;
    while((i<size-1)&&(c!='\n'))
    {
        n = recv(sock, &c, 1, 0);
        if(n>0)
        {
            if(c=='\r')
            {
                /*偷窥一个字符*/
                n = recv(sock, &c, 1, MSG_PEEK);
                if(n>0&&(c=='\n'))
                {
                    recv(sock, &c, 1, 0);
                }
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i]='\0';
    return i;
}

/*http的头部字段*/
void headers(int client, const char *filename)
{
    char buf[1024];
    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}
void bad_request(int client)
{
    char buf[1024];
    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

void cannot_execute(int client)
{
    char buf[1024];
    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

/*获取并发送文件的内容*/
void cat(int client, FILE *resource)
{
    char buf[1024];
    fgets(buf, sizeof(buf), resource);
    while(!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}
/*如果不是CGI文件，直接读取文件返回请求的http客户端*/
void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];
    buf[0]='a', buf[1]='\0';
    while(numchars>0&&strcmp("\n", buf))
    {
        numchars = get_line(client, buf, sizeof(buf));
    }
    resource = fopen(filename, "r");
    if(resource==NULL)
        not_found(client);
    else
    {
        headers(client, filename);
        cat(client, resource);
    }
    fclose(resource);
}
void execute_cgi(int client, const char *path, const char *method, const char *query_string)
{
    char buf[1024];
    /*用于父子进程间通信*/
    int cgi_output[2];
    int cgi_input[2];
    pid_t pid;
    int status;
    int i;
    char c;
    int numchars = 1;/*读取的字符*/
    int content_length = -1; /*http的content_length*/
    buf[0]='1', buf[1]='\0';
    if(strcasecmp(method,"GET")==0)
    {
        /*因为GET将URL放在请求行，所以余下的信息不需要分析了*/
        while(numchars>0&&strcmp("\n", buf))
        {
            numchars = get_line(client, buf, sizeof(buf));
        }
    }
    else
    {
        /*如果是POST方法，需要得到Content-Length:*/
        numchars = get_line(client, buf, sizeof(buf));
        while(numchars>0&&strcmp("\n", buf))
        {
            buf[15]='\0';
            if(strcmp(buf, "Content-Length:")==0)
            {
                content_length = atoi(&buf[16]);
            }
            numchars = get_line(client, buf, sizeof(buf));
        }
        if(content_length == -1)
        {
            bad_request(client);
            return ;
        }
    }
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    if(pipe(cgi_input)<0||pipe(cgi_output)<0)
    {
        cannot_execute(client);
        return;
    }
    if((pid=fork())<0)
    {
        cannot_execute(client);
        return;
    }
    if(pid == 0) /*child: excute CGI script*/
    {
        close(cgi_output[0]);
        close(cgi_input[1]);
        char meth_env[255];
        char query_env[255];
        char length_env[255];
        dup2(cgi_input[0], 0);
        dup2(cgi_output[1], 1);
        /*CGI环境变量*/
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);
        if(strcasecmp(method, "GET")==0)
        {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else
        {
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        execl(path, path, NULL);
        exit(0);
    }
    else
    {
        /*关闭无用管道*/
        close(cgi_output[1]);
        close(cgi_input[0]);
        if(strcasecmp(method, "POST")==0)
        {
            /*得到post请求数据，写到input管道中*/
            for(i=0;i<content_length;i++)
            {
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
            while(read(cgi_output[0], &c, 1)>0)
            {
                send(client, &c, 1, 0);
            }
        }
        close(cgi_output[0]);
        close(cgi_input[1]);
        waitpid(pid, &status, 0);
    }
}
/*处理请求*/
void accept_request(void *arg)
{
    int client = (int)arg;
    char buf[1024];
    int numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i=0, j=0;
    struct stat st;
    int cgi = 0; /*如果是CGI脚本，cig=1*/

    char *query_string = NULL;
    /*"GET /path HTTP/1.1/r/n"*/
    numchars = get_line(client, buf, sizeof(buf));/*处理请求行*/
    while(!ISspace(buf[j])&&(i<sizeof(method)-1))
    {
        method[i] = buf[j];
        i++;
        j++;
    }
    method[i]='\0';
    if(strcasecmp(method, "GET")&&strcasecmp(method, "POST"))
    {
        unimplemented(client);
        return;
    }
    /*如果是POST，cgi置为1*/
    if(strcasecmp(method, "POST") == 0)
        cgi = 1;
    i = 0;
    /*跳过空白符*/
    while(ISspace(buf[j])&&(j<sizeof(buf))) j++;
    /*解析url*/
    while(!ISspace(buf[j])&&(i<sizeof(url)-1)&&(j<sizeof(buf)))
    {
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';
    if(strcasecmp(method, "GET")==0)
    {
        query_string = url;
        while((*query_string!='?')&&(*query_string!='\0'))
            query_string++;
        if(*query_string=='?')
        {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }
    sprintf(path, "htdocs%s", url);
    /*GET / HTTP/1.1/r/n*/
    if(path[strlen(path)-1]=='/')
        strcat(path, "index.html");
    /*获取文件信息*/
    if(stat(path, &st)==-1)
    {
        /*把http请求报文剩余的信息读出然后丢弃*/
        while(numchars>0&&strcmp("\n", buf))
        {
            numchars = get_line(client, buf, sizeof(buf));
        }
        /*没有找到文件*/
        not_found(client);
    }
    else
    {
        if((st.st_mode&S_IFMT)==S_IFDIR)
        {
            strcat(path, "/index.html");
        }
        if((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
        {
            cgi = 1;
        }
        if(!cgi)
            serve_file(client, path);
        else
            execute_cgi(client, path, method, query_string);
    }
    close(client);
}
int main()
{
    int server_sock = -1;
    u_short port = 0;
    int client_sock = -1;
    struct sockaddr_in client_addr;
    socklen_t clientaddr_len = sizeof(client_addr);
    pthread_t newthread;
    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);
    while(1)
    {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &clientaddr_len);
        if(client_sock==-1)
            error_die("accept");
        /*每次接收到请求，就创建一个线程来处理*/
        if(pthread_create(&newthread, NULL, accept_request, (void *)client_sock)!=0)
            error_die("pthread_create");
    }
    close(server_sock);
    return 0;
}