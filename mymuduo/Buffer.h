#pragma once
#include <vector>
#include<string>
#include<algorithm>
// 网络库底层的缓冲区类型定义
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;    // buffer中，用来存储buffer大小的信息
    static const size_t kInittialSize = 1024; // buffer中，表示存储传输信息的大小

    explicit Buffer(size_t initialSize = kInittialSize)
        : buffer_(kCheapPrepend + kInittialSize), // 总空间就是，存储buffer大小的信息 + 存储传输信息大小
          readerIndex_(kCheapPrepend), 
          writerIndex_(kCheapPrepend)
    {
    }

    // 可读数据的大小
    size_t readableByte() const
    {
        return writerIndex_ - readerIndex_;
    }

    // 可写数据的大小
    size_t writableBytes() const
    {
        return buffer_.size() - writerIndex_;
    }

    size_t prependableByte() const
    {
        return readerIndex_;
    }

    //返回缓冲区中可读数据的起始地址
    const char* peek() const
    {
        return begin()+readerIndex_;
    }

    // 将buffer 转换成 string类型
    void retrieve(size_t len)
    {
         if(len < readableByte())   //即没有一次性读完所有数据，只读了len个字节
         {
            readerIndex_ += len;  
         }
         else  // 即len == readableByte()，表示一次性读完了所有字节
         {
            retrieveAll();
         }
    }

    void retrieveAll() 
    {
        readerIndex_ = writerIndex_ =kCheapPrepend;
    }

    // 把onMessage函数上报的Buffer数据，转成string类型的数据返回 
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableByte());  //应用可读取数据的长度
    }

    std::string retrieveAsString(size_t len)
    {
        //将缓冲区中的可读数据，读取出来
        std::string result(peek(),len);
        retrieve(len);
        return result;
    }

    //判断是否有足够空间写 
    void ensureWriteableBytes(size_t len)
    {
        if(writableBytes() < len) 
        {
            makeSpace(len);  //扩容函数
        }
    }

    //从fd上读取数据
    ssize_t readFd(int fd,int* saveErrno);

    //往fd上写数据
    ssize_t writeFd(int fd,int* saveErrno);

        //把[data, data + len]内存上的数据，添加到writable缓冲区当中 
    void append(const char *data,size_t len)
    {
        ensureWriteableBytes(len);
        std::copy(data,data+len,beginWrite());
        writerIndex_ += len;
    }
private:
    char *begin()
    {
        return &*buffer_.begin();
    }

    const char *begin() const
    {
        return &*buffer_.begin();
    }

    void makeSpace(size_t len)
    {
        /*
        kCheapPrepend  |  reader  |  writer  |
        kCheapPrepend  |        len          |
        */
       if(writableBytes() + prependableByte() < len + kCheapPrepend)
       {
        buffer_.resize(writerIndex_ + len);
       }
       else
       {
        //说明 写空间和前面的剩余空间足够，就把read空间未读的数据 往 读完的地方放
        size_t readable = readableByte();
        std::copy(begin()+readerIndex_,begin()+writerIndex_,begin()+kCheapPrepend);
        readerIndex_ = kCheapPrepend;
        writerIndex_ = readerIndex_ + readable;

       }
    } 

    char* beginWrite()
    {
        return begin() + writerIndex_;
    }

    const char* beginWrite() const
    {
        return begin() + writerIndex_;
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};