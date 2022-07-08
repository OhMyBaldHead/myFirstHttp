#ifndef _THREAD_POOL_SERVER_H
#define _THREAD_POOL_SERVER_H

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<unistd.h>
#include<pthread.h>
#include<sys/epoll.h>
#include<error.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<syslog.h>

//*******************************线程池部分**********************************//

/* 存放响应报文内容的结构体 */
typedef struct _DataEvent{
    int cfd;                        //client 套接字
    int state;                      //标记是否在红黑树上
    char path[256];                 //存放get路径
}DataEvent;

/* IP地址结构体 */
typedef struct _Cfd_IP{
    char client_ip[64] ;            //定义IP 地址字符串
}Cfd_IP;

/* 线程池任务结构体 */
typedef struct _MyTask{
    void (*task_func)(void *);      //任务回调函数
    void * arg;                     //回调函数参数
    int lfd;                        //监听套接字
    int epfd;                       //epoll红黑树fd
    struct epoll_event ev;          //epoll_event       
    DataEvent* data_events;         //存储响应报文的数组
}MyTask;

/* 县城池结构体 */
typedef struct _MyThreadPool{
    pthread_mutex_t lock;           //整个结构体锁
    pthread_cond_t not_full;        //”任务队列不为满“条件变量
    pthread_cond_t not_empty;       //“任务队列不为空”条件变量

    pthread_t *work_tid;            //线程tid数组
    MyTask *queue_task;             //任务队列

    int num_threads;                //线程数目
    int num_task;                   //当前任务数目
    int num_max_task;               //最大任务数目
    int queue_front;                //任务队首
    int queue_end;                  //任务队尾
    int shutdown;                   //线程池是否关闭标志位
}MyThreadPool;


/* 全局变量IP */
Cfd_IP cfd_ip[1024]; 

/* 线程池的创建与初始化函数 */
MyThreadPool *MyTP_create(int max_threads,int max_task);

/* 线程池的销毁函数 */
void MyTP_destory(MyThreadPool * My_TP);

/* 线程池的添加队列函数 */
void MyTP_add(void * arg,int lfd,int epfd,struct epoll_event *ev,DataEvent* data_events,void(*func)(void *));

/* 线程的回调函数 */
void MyTP_work(void *arg);

/* 读写任务回调函数 */
void task_work_read_write(void * arg);

/* 监听任务回调函数 */ 
void task_work_listen(void * arg);




//*******************************http服务器部分**********************************//

/* 错误显示并退出函数 */
void Perror(const char * s);

/* 读取一行的操作 以\r\n结束 */
int get_line(int cfd, char *buf, int size);

/* 初始化套接字并将lfd放到红黑树上 */
int inite_listen_fd(int efd);

/* 将监听的cfd挂上树操作 */
void do_accept(int efd,int lfd);

/* 服务器读事件函数 */
void do_read(int cfd,int efd,DataEvent* data_events);

/* 向缓冲区发送数据函数 */
void do_send(int cfd,int efd,void* ptr);

/* GET请求回复函数 */
void http_request_get(int cfd,const char *file);

/* 相应报文状态行 */
void send_respond(int cfd,int re_num,char *disp,char *type,int len);

/* 发送响应报文body */
void send_file(int cfd,const char *file);

/* 断开连接函数 */
void dis_connect(int cfd,int efd);

/* 服务器运行 */
void Server_run();

#endif