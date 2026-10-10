#include "Connection.h"
#include <sys/epoll.h>
#include <unistd.h> // 提供 close(), write(), ssize_t
#include <cerrno>   // 提供 errno, EINTR (C++ 推荐风格，等价于 <errno.h>)

void Connection::handleRead()
{
    // client_fd
    // 读取client_fd中的数据
    char buf[1024] = {};
    ssize_t n = ::read(channel_.getFd(), buf, sizeof(buf));
    if (n > 0)
    {
        input_.append(buf, n);
        // 触发处理命令的回调函数
        if (on_message_)
            on_message_(*this);
    }
    else if (n == 0)
    {
        // 对端关闭
        if (on_close_)
            on_close_(*this);
    }
    else if (n == -1 && (errno == EINTR || errno == EAGAIN))
    {
        // 下次再来
        return;
    }
    else
    {
        // error
        if (on_close_)
            on_close_(*this);
    }
}

Connection::Connection(int fd, uint32_t events) : socket_(fd), channel_(fd, events)
{
    // 绑定读事件回调函数
    channel_.setReadCb([this]()
                       { handleRead(); });

    // 绑定写事件回调函数
    channel_.setWriteCb([this]()
                        {
                            // client_fd 写入数据
                            send(); });
    channel_.setCloseCb(
        [this]()
        {
            if (on_close_)
                on_close_(*this);
        }

    );
}
// getChannel
Channel &Connection::getChannel() { return channel_; }
// getFd
int Connection::getFd() const { return channel_.getFd(); }
// 设置业务处理的回调函数
void Connection::setOnMessage(std::function<void(Connection &)> cb) { this->on_message_ = cb; }
// 设置业务处理失败的回调函数
void Connection::setOnClose(std::function<void(Connection &)> cb) { this->on_close_ = cb; }
// 设置将监听事件修改为读事件的回调函数
void Connection::setModEpoll_EPOLLIN(std::function<void(Connection &)> mee) { modEpoll_EPOLLIN_ = mee; }

void Connection::outputAppend(const char *data, size_t len) { output_.append(data, len); } // 写缓冲区追加
std::string_view Connection::inputReadableView() { return input_.readableView(); }         // 读取读缓冲区
std::string_view Connection::outputReadableView() { return output_.readableView(); }       // 读取写缓冲区
void Connection::inputRetrieve(size_t n) { input_.retrieve(n); }                           // “删除”写缓冲区“已读”部分
void Connection::outputRetrieve(size_t n) { output_.retrieve(n); }                         // “删除”读缓冲区“已读”部分

// 将写缓冲区内的缓存写出，并且“删除已读”
void Connection::send()
{
    ssize_t sent = write(channel_.getFd(), output_.readableView().data(), output_.readableView().size());
    if (sent >= 0)
    {
        outputRetrieve(sent);
        // 已经将这个连接内的output写缓冲区读完，可以将监听 读|写 事件改成监听读事件
        if (output_.readable() == 0 && channel_.getEvents() != EPOLLIN)
        {
            if (modEpoll_EPOLLIN_)
                modEpoll_EPOLLIN_(*this);
        }
    }
    else if (sent == -1 && (errno == EINTR || errno == EAGAIN))
    {
        // 信号中断，重试
        return;
    }
    else
    {
        // 由于字节流错误，导致连接中断，无法继续write
        if (on_close_)
            on_close_(*this);
        return;
    }
}