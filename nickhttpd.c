#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#define ISspace(x) isspace((int)(x))//定义一个判断是否为空格的宏
#define SERVER_STRING "Server: nickxueweihttpd/0.1.0\r\n"
#define NOT_FOUND "HTTP/1.0 404 NOT FOUND\r\nServer: nickxueweihttpd/0.1.0\r\nContent-Type: text/html\r\n\r\n<HTML><TITLE>Not Found</TITLE>\r\n<BODY><P>The server could not fulfill\r\nyour request because the resource specified\r\nis unavailable or nonexistent.</P>\r\nWe cannot find the file!\r\n</BODY></HTML>\r\n"
#define HEADER "HTTP/1.0 200 OK\r\nServer: nickxueweihttpd/0.1.0\r\n"
#define STDIN   0
#define STDOUT  1
#define STDERR  2
char iloveyou[1024];
void accept_request(void *arg);
void serve_file(int client, const char *filename);
void unimplemented(int client);
int startup(u_short *port);
int get_line(int sock, char *buf, int size);

int main(void)
{
    //定义一个我们放文件的绝对地址
    strcpy(iloveyou,"/home/nick/httpd");
    int server_sock = -1;
    u_short port = 80;//定义端口为80
    int client_sock = -1;
    struct sockaddr_in client_name;//IP地址结构
    socklen_t  client_name_len = sizeof(client_name);
    pthread_t newthread;

    server_sock = startup(&port);//startup函数建立socket

    while (1)
    {
        client_sock = accept(server_sock,
                (struct sockaddr *)&client_name,
                &client_name_len);//建立连接
        if (client_sock == -1){
            printf("accept failed!\n");
            return EXIT_FAILURE;
        }
        //下面建立处理信息的线程
        pthread_create(&newthread , NULL, (void *)accept_request, (void *)(intptr_t)client_sock);
        //pthread_detach(newthread);//异步进行
    }

    close(server_sock);

    return(0);
}

void accept_request(void *arg){
    int client = (intptr_t)arg;//这个应该就是请求的http头
    char buf[1024];//用来读取字符串的缓存
    size_t numchars;
    char method[255];//用来储存头里面的方法
    char url[255];//用来储存url
    char path[512];//文件地址
    size_t i,j,ok;
    struct stat st;
    

    char *query_string = NULL;//本来是用来判断get请求中有没有问号的（有的话就是一个带参数的get请求，实际上要按照post来处理），现在暂时不管他

    //我们首先从中看请求方法
    numchars = get_line(client,buf,sizeof(buf));//读取请求头，请求头都是以回车换行结尾的
    i=0;j=0;
    //printf("让我们看看buf是什么: %s\n",buf);
    //第一个空格出现之前的就是请求方法
    while(!ISspace(buf[i]) && (i<sizeof(method)-1)){
        method[i]=buf[i];
        i++;
    }
    j=i;//记录读到请求方法之后的位置
    method[i]='\0';

    //然后我们进行获取url
    i=0;//i用来记录url的长度
    while(ISspace(buf[j]) && (j<numchars))j++;//首先把方法和url之间的空格们都过滤掉
    while(!ISspace(buf[j]) && (j<numchars) && (i<sizeof(url)-1)){
        url[i]=buf[j];
        i++;
        j++;
    }
    url[i]='\0';

    //然后我们判断请求方法
    if(strcasecmp(method,"GET")==0){
        //现在要处理文件路径了
        //printf("让我们看看url是什么: %s\n",url);
        //我们规定的url格式是"/xxx/yyy"，xxx是文件夹地址（有txt,img,html三种），yyy是文件名称（test.txt，logo.jpg，test.html，noimg.html）
        //其实不需要处理，只需要把他们加到我们的绝对地址之后就ok了
        //绝对地址我们定义在main函数里面
        strcpy(path,iloveyou);
        strcat(path,url);
        printf("让我们来看看文件地址对不对: %s %c\n",path,path[strlen(path)-1]);
        //然后我们把文件地址传给响应函数
	if(path[strlen(path)-1]!='l' && path[strlen(path)-1]!='g' && path[strlen(path)-1]!='t'){
		memset(buf,0,sizeof(buf));
		sprintf(buf,NOT_FOUND);
		send(client,buf,strlen(buf),0);
	}
	else serve_file(client,path);
        printf("传输字节完毕！\n");
    }
    else if(strcasecmp(method,"POST")==0){
        printf("留给薛伟完成");
    }
    else{
        printf("不支持这种请求方法，要报错返回给浏览器\n");
        unimplemented(client);
    }
}

void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];
    int lenth=0;
    char len[100];
    int i=0,count=0;
    char c;

    buf[0] = 'A'; buf[1] = '\0';
    //首先把请求全部读取然后丢掉
    while ((numchars > 0) && strcmp("\n", buf))  
        numchars = get_line(client, buf, sizeof(buf));

    printf("0\n");
    //然后打开文件开始读取
    resource = fopen(filename, "rb");
    printf("0.1\n");
    if (resource == NULL){
        //如果没找到，返回notfound
        sprintf(buf, NOT_FOUND);
        send(client, buf, strlen(buf), 0);
	printf("Cannot find!\n");
    }
    else
    {
	printf("Find!\n");
        //先获取文件的字节大小
        FILE *fp = resource;
        fseek(fp,0,SEEK_END);
        lenth = ftell(fp);
        sprintf(len,"Content-Length: %d\r\n",lenth);
        fclose(resource);
        resource = fopen(filename, "rb");
        printf("1\n");
        //首先要判断文件类型，传输对应的响应头（文件类型不同）
        if(filename[strlen(filename)-1]=='l'){
            sprintf(buf,HEADER);
            strcat(len,"Content-Type: text/html\r\n\r\n");
            strcat(buf,len);
            printf("让我们来看看我们的响应: %s\n", buf);
            send(client,buf,strlen(buf),0);
            //然后再把文件传过去
            printf("2\n");

            while(1){
                memset(buf,0,1024);
                i=fread(buf,1,1024,resource);
                send(client,buf,strlen(buf),0);
                count+=i;
                if(i!=1024)break;
            }
            
        }
        else if(filename[strlen(filename)-1]=='g'){
            sprintf(buf,HEADER);
            strcat(len,"Content-Type: application/x-jpg\r\n\r\n");
            strcat(buf,len);
            printf("让我们来看看我们的响应: %s\n", buf);
            send(client,buf,strlen(buf),0);
            //然后再把文件传过去
            printf("2\n");
            while ((fscanf(resource,"%c",&c))!=EOF)
            {
                printf("22\n");
                count++;
                send(client, &c, 1, 0);
            }
        }
        else if(filename[strlen(filename)-1]=='t'){
            sprintf(buf,HEADER);
            strcat(len,"Content-Type: text/plain\r\n\r\n");
            strcat(buf,len);
            printf("让我们来看看我们的响应: %s\n", buf);
            send(client,buf,strlen(buf),0);
            //然后再把文件传过去
            printf("2\n");

            while(1){
                memset(buf,0,1024);
                i=fread(buf,1,1024,resource);
                send(client,buf,strlen(buf),0);
                count+=i;
                if(i!=1024)break;
            }
            
        }
        else{
            sprintf(buf,NOT_FOUND);
            send(client,buf,strlen(buf),0);
        }
        
        printf("结束文件传输%d\n",count);
    	fclose(resource);
    }
}

//读取socket的一行，并以换行符结尾
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';
    return(i);
}

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
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

int startup(u_short *port)
{
    int httpd = 0;
    int on = 1;
    struct sockaddr_in name;//建立ip地址结构

    httpd = socket(PF_INET, SOCK_STREAM, 0);//建立socket
    if (httpd == -1){
        printf("socket create failed!\n");
        return EXIT_FAILURE;
    }
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if ((setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0)  
    {  
        printf("setsockopt failed\n");
        return EXIT_FAILURE;
    }
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0){
        printf("bind\n");
        return EXIT_FAILURE;
    }
    if (*port == 0)
    {
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1){
            printf("getsockname\n");
            return EXIT_FAILURE;
        }
        *port = ntohs(name.sin_port);
    }
    if (listen(httpd, 5) < 0){
        printf("listen\n");
        return EXIT_FAILURE;
    }
    return(httpd);
}
