#pragma once
#include <cstdint>
#include <functional>
#include "FdGuard.h"
#include "Channel.h"
class Acceptor
{
public:
    Acceptor(uint16_t port, int backlog);

    Acceptor(const Acceptor &) = delete;
    Acceptor &operator=(const Acceptor &) = delete;

    Channel &getChannel();
    void handleRead();
    void setNewConnectionCb(std::function<void(int client_fd)> nccb);

private:
    FdGuard socket_;
    Channel channel_;
    std::function<void(int client_fd)> new_connection_cb_;
};