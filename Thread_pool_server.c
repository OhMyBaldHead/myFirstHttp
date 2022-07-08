#include"Thread_pool_server.h"

#define PORT 8888                  //定义端口号


/*******************************************/
/*线程池的创建与初始化函数
*参数max_threads 线程池线程个数
*参数max_task 线程池最大任务数
*返回值 创建的线程池指针
/*******************************************/
MyThreadPool *MyTP_create(int max_threads,int max_task){
    //printf("MyTP_create函数调用*****\n");
    MyThreadPool * My_TP = NULL;
    My_TP = (MyThreadPool *)malloc(sizeof(MyThreadPool));                        //为线程池申请空间
    My_TP->queue_task = (MyTask *)malloc(sizeof(MyTask)*max_task);               //为任务数组申请空间
    My_TP->work_tid = (pthread_t *)malloc(sizeof(pthread_t)*max_threads);        // 为进程tid数组开辟空间
    if(My_TP == NULL || My_TP->queue_task == NULL || My_TP->work_tid == NULL){   //错误处理
        printf("malloc pthread_pool fail !\n");
        MyTP_destory(My_TP);
        return NULL;
    }
    //初始化锁和条件变量
    int re1 = pthread_mutex_init(&(My_TP->lock),NULL);
    int re2 = pthread_cond_init(&(My_TP->not_empty),NULL);
    int re3 = pthread_cond_init(&(My_TP->not_full),NULL);
    if(re1 == -1 || re2 == -1 ||re3 == -1){
        printf("pthread_mutex,cond inite fail!\n");
        MyTP_destory(My_TP);
        return NULL;
    }
    //初始化int型变量
    My_TP->num_max_task = max_task;
    My_TP->num_task = 0;
    My_TP->num_threads = max_threads;
    My_TP->queue_front = 0;
    My_TP->queue_end = 0;
    My_TP->shutdown = 0;  //表示线程池开启

    //建立相应数量的线程
    for(int i = 0;i<max_threads;i++){
        int re = pthread_create(&(My_TP->work_tid[i]),NULL,MyTP_work,(void *)My_TP);
        if(re == -1){
            printf("pthread_create error!\n");
            MyTP_destory(My_TP);
            return NULL;
        }
        else{
            //printf("pthread:%d is builded!\n",i);
        }
    }
    return My_TP;

}

/*******************************************/
/*线程池的销毁函数
*参数My_TP 线程池结构体引用
/*******************************************/
void MyTP_destory(MyThreadPool * My_TP){
    //printf("MyTP_destory函数调用*****\n");
    if(My_TP == NULL){
        printf("线程池已经销毁！\n");
        return;
    }
    
    My_TP->shutdown = 1;                          // 让线程池结束的标志位置1
    pthread_cond_broadcast(&(My_TP->not_empty));  //唤醒所有进程，使之自动结束  


    sleep(5);                                     //用5秒的时间等待其他线程处理完任务
    
    //销毁三个锁变量
    pthread_mutex_destroy(&(My_TP->lock));
    pthread_cond_destroy(&(My_TP->not_empty));
    pthread_cond_destroy(&(My_TP->not_full));

    //释放三个空间
    free(My_TP->queue_task);
    free(My_TP->work_tid);
    free(My_TP);
}

/*******************************************/
/*给线程池的任务队列添加任务函数
*参数arg 县城池指针
*参数lfd epoll的listen fd
*参数epfd epoll的红黑树文件描述符
*参数ev epoll的cfd的epoll_event
*参数data_events 用于记录响应内容的ptr结构体
*参数func 回调函数
/*******************************************/
void MyTP_add(void * arg,int lfd,int epfd,struct epoll_event *ev,DataEvent* data_events,void(*func)(void *)){
    //printf("MyTP_add函数调用*****\n");
    MyThreadPool * My_TP = (MyThreadPool *)arg;
    //操作先加锁  
    pthread_mutex_lock(&(My_TP->lock));
    //判断队列是否是满的
    while(My_TP->num_task >= My_TP->num_max_task){
        //printf("cfd:%d add waiting...",ev->data.fd);
        pthread_cond_wait(&(My_TP->not_full),&(My_TP->lock));
    }
    //给队尾元素添加任务 
    My_TP->queue_task[My_TP->queue_end].arg = (void *)&(My_TP->queue_task[My_TP->queue_end]);
    My_TP->queue_task[My_TP->queue_end].epfd = epfd;
    My_TP->queue_task[My_TP->queue_end].ev = *ev;                  //将ev的内容用值拷贝的方式拿到
    My_TP->queue_task[My_TP->queue_end].lfd = lfd;
    My_TP->queue_task[My_TP->queue_end].data_events = data_events;
    //给任务添加响应的任务处理函数
    My_TP->queue_task[My_TP->queue_end].task_func = func;
    My_TP->queue_end = (My_TP->queue_end+1)%My_TP->num_max_task;   //队尾元素++
    My_TP->num_task++;                                             //任务数++
    pthread_mutex_unlock(&(My_TP->lock));                          //解锁
    pthread_cond_signal(&(My_TP->not_empty));                      //唤醒一个线程进行处理
}

/*******************************************/
/*线程的回调函数
*参数arg 线程池指针
/*******************************************/
void MyTP_work(void *arg){
    //获得任务，处理任务
    MyThreadPool * My_TP = (MyThreadPool *)arg;
    MyTask task;    //为任务的执行单独开辟任结构体空间

    while(1){
        pthread_mutex_lock(&(My_TP->lock));
        while(My_TP->num_task == 0 && My_TP->shutdown == 0){                      //线程池没有关闭并且任务队列没有任务
            pthread_cond_wait(&(My_TP->not_empty),&(My_TP->lock));                //阻塞监听not_empty
        }
        //printf("MyTP_work函数调用*****\n");
        //如果任务队列不为空 拿走一个任务 并更新任务队列
        if(My_TP->num_task > 0){
            task = My_TP->queue_task[My_TP->queue_front];                          //将任务取出到task中
            memset(&(My_TP->queue_task[My_TP->queue_front]),0,sizeof(MyTask));     //将任务数组中的取出的任务置零
            My_TP->queue_front = (My_TP->queue_front +1)%My_TP->num_max_task;      //将任务下标增加

            //处理task->arg参数 处理 num_task
            task.arg = (void *)&task;
            My_TP->num_task--;
            //如果add处于阻塞态，就通过not_full 将add唤醒
            pthread_cond_signal(&(My_TP->not_full));
        }

        //如果shutdown标志位为1 处理善后工作并退出线程
        if(My_TP->shutdown == 1){
            pthread_mutex_unlock(&(My_TP->lock));
            pthread_exit(NULL);
        }

        //自己处理  task结构体事件 并解锁
        pthread_mutex_unlock(&(My_TP->lock));
        task.task_func(task.arg);
    }
}

/*******************************************/
/*读和写任务的回调函数
*参数arg 任务结构体指针
/*******************************************/
void task_work_read_write(void * arg){
    MyTask task_now = *(MyTask *)arg;           //接受任务的结构体
    //printf("task_work_read_write*****\n");

    if(task_now.ev.events & EPOLLIN){           //如果是读事件
        do_read(task_now.ev.data.fd,task_now.epfd,task_now.data_events);   
    }
    else{                                       //如果是写事件
        do_send(((DataEvent*)(task_now.ev.data.ptr))->cfd,task_now.epfd,task_now.ev.data.ptr);
    }
    return;

}


/*******************************************/
/*监听任务的回调函数
*参数arg 任务结构体指针
/*******************************************/
void task_work_listen(void * arg){
    MyTask task_now = *(MyTask *)arg;           //接受任务结构体
    //printf("task_work_listen*****\n");
    do_accept(task_now.epfd,task_now.lfd); 
    return;

}





/*******************************************/
/*显示错误并且退出
*参数s 错误字符串
/*******************************************/
void Perror(const char * s){
    perror(s);
    exit(1);
}

/*******************************************/
/*读取http请求的报文的一行，每一行用\r\n结束
*参数cfd client fd文件描述符
*参数buf 缓冲区
*参数size 缓冲区大小
*返回值 读取到的字节数
/*******************************************/
int get_line(int cfd, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;                                                 
    while ((i < size-1) && (c != '\n')) {         //while循环在读到buf满的时候或者没有数据的时候退出
        n = recv(cfd, &c, 1, 0);                  //接收一个字符
        if (n > 0) {     
            if (c == '\r') {            
                n = recv(cfd, &c, 1, MSG_PEEK);   //预读取的功能
                if ((n > 0) && (c == '\n')) {              
                    recv(cfd, &c, 1, 0);
                } else {                       
                    c = '\n';                    
                }
            }
            buf[i] = c;
            i++;
        } else {      
            c = '\n';
        }
    }
    buf[i] = '\0';
    if (-1 == n)
    	i = n;
    return i;
}

/*******************************************/
/*根据文件名字自动推导文件类型
*参数name 文件名字
*返回值 文件类型
/*******************************************/
const char *get_file_type(const char *name)
{
    char* dot;
    // 自右向左查找‘.’字符, 如不存在返回NULL
    dot = strrchr(name, '.');   
    if (dot == NULL)
        return "text/plain; charset=GB2312";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=GB2312";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp( dot, ".wav" ) == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";
    return "text/plain; charset=GB2312";
}


/*******************************************/
/*初始化套接字并将lfd放到红黑树上
*参数efd epoll红黑树的文件描述符号
/*******************************************/
int inite_listen_fd(int efd){
    int lfd= 0 ;
    //创建套接字 socket 选择IPv4 和 TCP 协议
    lfd = socket(AF_INET,SOCK_STREAM,0); 
    if(lfd == -1){         //创建失败
        Perror("socket create lfd error!");
    }

    //创建服务器地址结构，IP + 端口号
    struct sockaddr_in addr_in;
    bzero(&addr_in,sizeof(addr_in));  // 初始化
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(PORT);
    addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    //设置IO复用
    int opt = 1;
    setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    //bind()
    int bb = bind(lfd,(struct sockaddr *)&addr_in,sizeof(addr_in));
    if(bb == -1){         //创建失败
        Perror("bind error!");
    }
    //Listen()   最大监听队列
    listen(lfd,1024);

    //创建epoll EVENT结构体
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = lfd;          //void *ptr = NULL
    //将lfd挂上树
    int ret = epoll_ctl(efd,EPOLL_CTL_ADD,lfd,&ev);
    if(ret == -1){
        Perror("epoll_ctl add lfd error");
    }
    return lfd;
    //printf("创建套接字lfd并将lfd挂上树！\n");
}


/*******************************************/
/*将监听的cfd挂上树操作
*参数efd epoll红黑树的文件描述符号
*参数lfd listen fd
/*******************************************/
void do_accept(int efd,int lfd){
    struct sockaddr_in c_addr;
    int c_addr_len = sizeof(c_addr);
    //创建新的cfd
    int cfd = accept(lfd,(struct sockaddr *)&c_addr,&c_addr_len);
    if(cfd == -1){
        Perror("accept error!");
    }

    inet_ntop(AF_INET, &c_addr.sin_addr.s_addr, cfd_ip[cfd].client_ip, sizeof(cfd_ip[cfd].client_ip));
    //设置非阻塞
    int flag = fcntl(cfd,F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd,F_SETFL,flag);
    struct epoll_event ev;
    //设置ET触发模式
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = cfd;
    int re = epoll_ctl(efd,EPOLL_CTL_ADD,cfd,&ev);
    if(re == -1){
        Perror("epoll_ctl add cfd error!");
    }
    printf("有客户端请求连接，创建新的cfd:%d并将cfd挂上树！\n",cfd);
}


/*******************************************/
/*读取buf的内容
*参数cfd client fd
*参数efd epoll文件描述符
*参数data_events 记录http响应内容的ptr结构体指针
/*******************************************/
void do_read(int cfd,int efd,DataEvent* data_events){
    char line[1024] = {0};
    char method[16],path[256],protocol[16];
    int len = get_line(cfd,&line,sizeof(line));  //读取http请求的第一行
    if(len == 0){                                //服务器检查到客户端关闭
        dis_connect(cfd,efd);
    }
    else{                                        //将http请求进行拆解，然后作出回应
        //拆解信息
        sscanf(line, "%[^ ] %[^ ] %[^ ]", method, path, protocol);	 
        printf("接受的客户端IP为：%s\n",cfd_ip[cfd].client_ip); 
		printf("接受的http协议为:method=%s, path=%s, protocol=%s\n", method, path, protocol);   
        char log[1024];
        sprintf(log,"客户端IP:%s method=%s path=%s protocol=%s",cfd_ip[cfd].client_ip,method,path,protocol);
        syslog(LOG_INFO, "请求 %s\n",log);
        while(len>0){                            //将传输协议中剩下的内容读走
            len = get_line(cfd,&line,sizeof(line));  
            if(line[0] == '\n') break;           //最后一行是大小为1的换行符\n
        }
        //判断请求报文类型
        //GET类型
        if(strncasecmp(method,"GET",3) == 0){
            //如果是GET 请求

            //定义struct epoll_event ev结构体
            struct epoll_event epv = {0, {0}};
            epv.events = EPOLLOUT | EPOLLET;
            //寻找一个不在树上的ptr指针
            int i = 0;
            while(i<1024 && data_events[i].state == 1) i++;
            if(i == 1024){
                 dis_connect(cfd,efd);           //如果没找到，说明读事件已经满了，就直接关闭此次通信
            }  
            else{                                //如果找到合适的ptr
                epv.data.ptr = (void*)&data_events[i];
                //ptr的初始化
                data_events[i].cfd = cfd;
                strcpy(data_events[i].path,path);
                data_events[i].state = 1;        //标记此ptr已经在树上了

                int re_mod = epoll_ctl(efd,EPOLL_CTL_MOD,cfd,&epv);  //修改cfd为EPOLLOUT 并且ptr->path =路径
            }
        //     char *file = path+1;
        //    // 如果没有指定访问的资源, 默认显示资源目录中的内容
        //     if(strcmp(path, "/") == 0){    
        //        // file的值, 资源目录的当前位置
        //         file = "main.html";
        //     }
        //     http_request_get(cfd,file);
        //     dis_connect(cfd,efd);
        }
    }
}

/*******************************************/
/*向客户端发送数据 函数内执行send()函数
*参数cfd client fd
*参数efd epoll文件描述符
*参数ptr epoll_event的ptr指针
/*******************************************/
void do_send(int cfd,int efd,void* ptr){
    DataEvent* data_event_now = (DataEvent*)ptr;
    char *file = data_event_now->path+1;
    // 如果没有指定访问的资源, 默认显示资源目录中的内容
    if(strcmp(data_event_now->path, "/") == 0){    
        // file的值, 资源目录的当前位置
        file = "main.html";
    }
    http_request_get(cfd,file);
    data_event_now->state = 0;         //标记这个ptr已经不在树上了 可以被其他的使用
    dis_connect(cfd,efd);

}

/*******************************************/
/*向客户端发送get响应报文
*参数cfd client fd
*参数file 文件路径
/*******************************************/
void http_request_get(int cfd,const char *file){
    struct stat sbuf;
    char *type = get_file_type(file);
    //判断文件是否存在
    int ret = stat(file,&sbuf);
    if(ret != 0){
        //回复浏览器404错误
        printf("文件不存在！\n");
        send_respond(cfd,404,"Not Found",".html",-1);
        send_file(cfd,"404.html");
    }
    else{
        //判断是否是一个普通文件
        if(S_ISREG(sbuf.st_mode)){
            send_respond(cfd,200,"OK",type,-1);
            send_file(cfd,file);
        }
    }
}


/*******************************************/
/*向客户端发送响应头的具体内容
*参数cfd client fd
*参数fre_num 响应体的字节数
*参数disp 响应描述
*参数type 文件类型
*参数len 响应体长度
/*******************************************/
void send_respond(int cfd,int re_num,char *disp,char *type,int len){
    char buf[4096] = {0};
    char log[1024];
    sprintf(log,"客户端IP:%s 响应号=%d 响应描述=%s 文件类型=%s",cfd_ip[cfd].client_ip,re_num,disp,type);
    syslog(LOG_INFO, "响应 %s\n",log);
    printf("响应报文如下：\n");
    sprintf(buf,"HTTP/1.1 %d %s\r\n",re_num,disp);   //应答报文第一行            如果响应报文拼写错误，那么html文件会下载，而不是直接打开
    printf("HTTP/1.1 %d %s\r\n",re_num,disp);        //应答报文第一行
    send(cfd,buf,strlen(buf),0);
    sprintf(buf,"Content-type:%s\r\n",type);
    printf("%s\r\n",type);
    sprintf(buf+strlen(buf),"Content-Length: %d\r\n",len);
    printf("Content-Length: %d\r\n",len);
    send(cfd,buf,strlen(buf),0);
    send(cfd,"\r\n",2,0);
}

/*******************************************/
/*向客户端发送响应体的具体内容
*参数cfd client fd
*参数file 文件路径
/*******************************************/
void send_file(int cfd,const char *file)
{
    int n = 0,ret;
    char buf[4096] = {0};
    //打开本地服务器文件，将文件通过buf+cfd 的方式传输给socket
    int fd = open(file,O_RDONLY);                     // 只读的方式打开文件
    if(fd == -1){

        Perror("open file error!");
    }
    
    while((n = read(fd,buf,sizeof(buf)))){
        int YY = 0;
        //判断cfd是否可写的语句
        // fd_set set_write;
        // select(0,NULL,&set_write,NULL,-1);
        // YY = FD_ISSET(fd,&set_write) ;
        int ret = send(cfd,buf,n,0);
        if(ret == -1){
            //printf("-----send ret:%d cfd:%d\n",ret,cfd);
            Perror("send file error!");
            
        }
        if(ret < 4096){
            //printf("-----send ret:%d cfd:%d\n",ret,cfd);
        }
    }
    close(fd);
}

/*******************************************/
/*断开连接
*参数cfd client fd
*参数efd epoll线程池描述符
/*******************************************/

void dis_connect(int cfd,int efd){
    int re = epoll_ctl(efd,EPOLL_CTL_DEL,cfd,NULL);
    if(re == -1){
        Perror("epoll_ctl delete cfd error!");
    }
    printf("客户端关闭连接，将cfd:%d从树上摘下！\n",cfd);
    close(cfd);
}

/*******************************************/
/*服务器运行函数
/*******************************************/
void Server_run(){

    printf("服务器启动！\n");
    //打开系统日志
    openlog("TestLog", LOG_CONS| LOG_PID, LOG_LOCAL0);
    //setlogmask(LOG_UPTO(LOG_NOTICE)); //设置屏蔽低于NOTICE级别的日志信息

    //起10个线程来阻塞等待读写事件
    MyThreadPool *My_TP = MyTP_create(10,10);                  
    DataEvent data_event[1024];                     //创建1024个读事件 void * ptr
    //初始化data_event[i]
    for(int i = 0;i<1024;i++){
        data_event[i].cfd = 0;
        strcpy(data_event[i].path,"");
        data_event[i].state = 0;                    //标记可以被用作void* ptr
    }

    //起5个线程来阻塞等待监听事件
    MyThreadPool *My_TP_accept = MyTP_create(5,5);                    
    DataEvent data_event_accept[1024];              //创建1024个没有用的事件 void * ptr

    //改变进程目录
    int re_chdir =  chdir("./source");
    if(re_chdir == -1){
        Perror("chdir error!\n");
    }

    //创建红黑树efd
    int efd = epoll_create(1024);
    int lfd = inite_listen_fd(efd);
    struct epoll_event get_events[1024];
    while(1){
        //printf("阻塞监听efd...\n");
        int ret = epoll_wait(efd,&get_events,1024,-1);
        if(ret == -1){
            Perror("epoll_wait error!");
        }
        //循环读取get_events
        for(int i = 0;i<ret;i++){
            //分辨lfd和cfd
            if(get_events[i].data.fd == lfd){
                //printf("event[%d]的fd = lfd\n",i);
                MyTP_add((void *)My_TP_accept,lfd,efd,&get_events[i],NULL,task_work_listen);
            }
            else{   //如果是cfd就读数据处理数据          //启动子线程来实现对cfd的响应
                    //用线程池来处理事件
                    // if(get_events[i].events & EPOLLIN)
                    // printf("event[%d]的fd = %d EPOLLIN\n",i,get_events[i].data.fd);
                    // if(get_events[i].events & EPOLLOUT)
                    // printf("event[%d]的fd = %d EPOLLOUT\n",i,((DataEvent*)(get_events[i].data.ptr))->cfd);
                    MyTP_add((void *)My_TP,lfd,efd,&get_events[i],data_event,task_work_read_write);
            }
        }
    }
    closelog();     //关闭系统日志
}
