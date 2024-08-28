# Mymuduo
本项目通过使用C++11标准语言对muduo网络库进行重写，使其不依赖于Boost库，可以通过运行./autobash.sh直接使用，不需要配置其他坏境，实现了网络层与业务层代码的分离。  
通过对muduo网络库的核心模块进行重写，可深入理解TCP协议和UDP协议、IO复用接口编程、LInux多线程编程、理解UNIX/Linux上的五种IO模型、select、poll、epoll优缺点、epoll原理和优势、Reactor模型等。  
# 主要工作
1. 编写Channel模块。当Channel模块封装了sockfd与其相关的事件处理函数和感兴趣的事件，每个Channel对象都注册到EventLoop中，当有事件发生时，EventLoop会通知对应的Channel进行处理。
2. 编写EventLoop模块。它负责事件循环的驱动，EventLoop内部维护了一个事件分发器Epoll，用于监听Channel上的事件。
3. 编写Thread、EventLoopThread、EventLoopThreadPool模块。Thread模块底层封装了C++11的thread，EventLoopThread模块封装了Thread和EventLoop，实现one Loop per Thread的设计理念，EventLoopThreadPool模块实现了线程池，避免了线程的频繁创建和销毁。
4. 编写Acceptor模块。Acceptor类负责监听指定的端口，并接受来自客户端的连接请求。当有新的连接请求到达时，Acceptor会创建一个新的TcpConnection对象，并将其与客户端进行关联。该模块运行在mainLoop中。
5. 编写Buffer模块。防止应用程序读写数据太快，而网络链路首发速度较慢，导致速度不匹配问题。
6. 编写TcpConnection模块。TcpConnection类代表了一个TCP连接。它封装了与客户端之间的通信逻辑，包括数据的读写、连接的保持和关闭等操作。
7. 编写TcpServer模块。TcpServer统领之前所有的类，管理着EventLoopThreadPool，保存所有的TcpConnection，用户提供的各种回调函数向TcpConnection的Channel中注册。它负责使用Acceptor监听指定的端口，并接受来自客户端的连接请求，为每个连接创建一个TcpConnection对象进行管理。
