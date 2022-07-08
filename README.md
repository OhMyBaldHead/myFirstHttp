# My First Http Server
本项目是用纯C语言编写的高并发轻量级服务器，可以相应不同种类的GET请求。采用Reactor高效模式，利用经典的epoll+线程池实现高并发。压力测试可以达到上万QPS。
# 环境要求
Ubuntu14   
gcc 
# 项目启动
gcc main.c Thread_pool.c -o main -pthread      
./main      
# 工作流程
1 服务器启动      
2 创建epoll红黑树 创建连接线程池 创建响应请求线程池     
3 初始化listen fd并将其挂上树    
4 阻塞监听epoll红黑树   
5 如果主线程有lfd反映，交给连接线程池分配线程创建cfd并挂上树    
6 如果主线程有cfd读事件反应，交给响应请求线程池分配线程处理读事件，之后将该cfd监听事件改为可写    
7 如果主线程有cfd可写事件反应，交给响应请求线程池分配线程处理写事件，之后将cfd从树上摘下，并关闭该cfd  
![7c1820210068e0251150893c62029ac](https://user-images.githubusercontent.com/93315922/177950660-336e7331-e249-49b1-b074-7fc8d1769bf1.png)
