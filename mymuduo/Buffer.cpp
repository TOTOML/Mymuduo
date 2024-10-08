#include "Buffer.h"
#include <errno.h>
#include <sys/uio.h>
#include<unistd.h>  //write函数的头文件
/*
 从fd上读取数据，Poller工作在LT模式
 Buffer缓冲区是由大小的！但是从fd上读数据的时候，却不知道tcp数据最终的大小
*/

ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    char extrabuf[65536] = {0}; // 栈上的空间   64KB
    struct iovec vec[2];
    const size_t writable = writableBytes(); // 底层Buffer中，缓冲区剩余的可写空间大小
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd,vec,iovcnt);
    if(n<0 )
    {
        *saveErrno = errno;
    }
    else if(n <= writable)  //说明Buffer本身的write缓冲区足够
    { 
        writerIndex_ += n ;
    }
    else   //说明Buffer本身缓冲区空间不够，在extrabuf中也存了数据
    {
        writerIndex_ = buffer_.size();
        append(extrabuf,n-writable); //writable为Buffer中的可写缓冲区大小，n-writable表示剩下还没写进Buffer写缓冲区的数据
    }

    return n;
}

ssize_t Buffer::writeFd(int fd,int *saveErrno)
{
    ssize_t n=::write(fd,peek(),readableByte());
    if(n<0)
    {
        *saveErrno = errno;
    }
    return n;
}
