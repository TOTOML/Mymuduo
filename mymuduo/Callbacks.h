#pragma once
#include <memory>
#include <functional>

class Buffer;
class TcpConnection;
class Timestamp;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr &)>;
using CloseCallback = std::function<void(const TcpConnectionPtr &)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;
//HighWaterMarkCallback用于控制发送发和接收方的处理速度，趋于一致，不然发送方太快，接收方太慢，会导致数据丢失
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr&,size_t)>;


using MessageCallback = std::function<void(const TcpConnectionPtr &,
                                           Buffer *,
                                           Timestamp)>;