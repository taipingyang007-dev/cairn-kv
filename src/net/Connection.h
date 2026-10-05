#pragma once
#include "Buffer.h"
#include "Channel.h"
#include "FdGuard.h"
class Connection
{
private:
    Buffer input_ = {};
    Buffer output_ = {};
    FdGuard socket_;
    Channel channel_;
    void handleRead();
    std::function<void(Connection &)> modEpoll_EPOLLIN_;
    std::function<void(Connection &)> on_message_ = {};
    std::function<void(Connection &)> on_close_ = {};
    // “删除”读缓冲区“已读”部分
    void outputRetrieve(size_t n);

public:
    Connection(int fd, uint32_t events);

    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    Connection(Connection &&) = delete;
    Connection &operator=(Connection &&) = delete;
    // ps:= default 时，编译器会做逐成员移动，对于Buffer可以进行移动构造，但是client_fd只能进行赋值拷贝
    // ps:内嵌的 Channel 已注册进 epoll，且 read_cb_ 捕获了 this —— 搬家会让两个指针同时悬空
    // getChannel
    Channel &getChannel();
    // getFd
    int getFd() const;
    // 写缓冲区追加
    void outputAppend(const char *data, size_t len);
    // 读取读缓冲区
    std::string_view inputReadableView();
    // 读取写缓冲区
    std::string_view outputReadableView();
    // “删除”写缓冲区“已读”部分
    void inputRetrieve(size_t n);
    // writeALL，向内核缓冲区全部写入
    void send();
    // 设置将监听事件修改为读事件的回调函数
    void setModEpoll_EPOLLIN(std::function<void(Connection &)> mee);
    // 设置业务处理的回调函数
    void setOnMessage(std::function<void(Connection &)> cb);
    // 设置业务处理失败的回调函数
    void setOnClose(std::function<void(Connection &)> cb);
};