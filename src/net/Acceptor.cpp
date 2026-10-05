#include "Acceptor.h"
// 网络系统调用相关
#include <sys/socket.h> // socket(), setsockopt(), bind(), listen(), AF_INET, SOCK_STREAM, SOL_SOCKET, SO_REUSEADDR
#include <sys/epoll.h>
#include <netinet/in.h> // sockaddr_in, INADDR_ANY, htons
#include <unistd.h>     // close()

// 基础 C/C++ 运行时
#include <cstdio>  // perror()
#include <cstdlib> // abort()
#include <netinet/tcp.h>

namespace
{
    int createListenFd(uint16_t port, int backlog)
    {
        // 创建socket
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1)
        {
            perror("socket failed:");
            abort();
        }

        // 开启端口复用（避免 TIME_WAIT 导致无法立即重启）
        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        {
            perror("setsockopt SO_REUSEADDR failed");
            abort();
        }

        // 配置地址并清零
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        // 绑定端口
        if (bind(server_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == -1)
        {
            perror("bind failed");
            abort();
        }

        // 监听
        if (listen(server_fd, backlog) == -1)
        {
            perror("listen failed");
            abort();
        }

        return server_fd;
    }
}

Acceptor::Acceptor(uint16_t port, int backlog) : socket_(createListenFd(port, backlog)), channel_(socket_.getFd(), EPOLLIN)
{
    channel_.setReadCb([this]()
                       { handleRead(); });
}

// 返回Channel
Channel &Acceptor::getChannel() { return channel_; }
// 创建客户端连接
void Acceptor::handleRead()
{
    // 有新的连接请求并设置非阻塞
    int client_fd = accept4(socket_.getFd(), nullptr, nullptr, SOCK_NONBLOCK);
    if (client_fd == -1)
    {
        perror("accept client_fd failed:");
        return;
    }

    int flag = 1; // 1 表示开启该选项，0 表示关闭,关闭 nagle 算法
    if (setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (void *)&flag, sizeof(flag)) == -1)
    {
        perror("setsockopt TCP_NODELAY failed");
        close(client_fd);
        return;
    }

    if (new_connection_cb_)
        new_connection_cb_(client_fd);
    else
        close(client_fd);
}

void Acceptor::setNewConnectionCb(std::function<void(int client_fd)> nccb) { new_connection_cb_ = std::move(nccb); }