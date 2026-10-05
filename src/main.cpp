#include <iostream>
#include <cstdio>
#include <csignal>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <unordered_map>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <memory>
#include <cassert>
#include <netinet/tcp.h>

#include "proto/RespParser.h"
#include "net/Connection.h"
#include "store/IStore.h"
#include "store/MemStore.h"
#include "store/RocksDBStore.h"
#include "cmd/commands.h"
#include "net/EventLoop.h"
#include "net/Acceptor.h"
#include "net/TimerQueue.h"
int main(int argc, char *argv[])
{
    // 忽略 SIGPIPE 信号，防止信号中断程序
    signal(SIGPIPE, SIG_IGN);

    // 创建Accept对象（socket，setsocketopt，bind，listen）
    Acceptor acpt(6380, 511);
    /*  创建EventLoop对象，
        包含poller,poller包含epoll_fd，注册(add,mod)，摘除(del)，wait(epoll_wait)
        包含std::vector<Channel *> active_channels;用于回调 */
    EventLoop loop{};
    loop.updateChannel(&acpt.getChannel());

    TimerQueue tq;
    loop.updateChannel(&tq.getChannel());
    TimerId id = tq.runAfter(std::chrono::seconds(2), []
                             { fprintf(stderr, "★ 不该出现\n"); });
    tq.runAfter(std::chrono::seconds(4), []
                { fprintf(stderr, "★ 4 秒到\n"); });
    tq.cancel(id); // 立刻取消最早的那个

    // 创建出（client_fd + Connection）map 表以存储连接句柄
    std::unordered_map<int, std::unique_ptr<Connection>> con_map;

    // 创建出 IStore
    std::unique_ptr<IStore> istore = {};
    std::string mode = (argc >= 2) ? argv[1] : "R"; // 默认值只出现一次

    if (mode == "M")
    {
        istore = std::make_unique<MemStore>();
    }
    else if (mode == "R")
    {
        istore = std::make_unique<RocksDBStore>("data");
    }
    else
    {
        std::cerr << "usage: " << argv[0] << " <M|R>\n"
                  << "  M   MemStore     纯内存，不持久化（压测对照组）\n"
                  << "  R   RocksDBStore 持久化到 ./data\n";
        return 1;
    }

    // 延迟close，避免在回调时出现 use-after-free
    std::vector<int> pending_close;
    auto on_close = [&pending_close](const Connection &conn)
    { pending_close.emplace_back(conn.getFd()); };

    // on_message 业务处理回调函数
    auto on_msg = [&istore, &on_close, &loop](Connection &conn)
    {
        // 读取命令并“删除”读缓冲区已读部分
        auto res = resp::parse(conn.inputReadableView());
        while (res.status == resp::ParseStatus::OK)
        {
            // “删除”读缓冲区已读部分
            conn.inputRetrieve(res.consumed);

            // 读取命令并进行相关的命令业务以及返回结果
            std::string reply = cmd::handleCommand(res.args, *istore);
            conn.outputAppend(reply.data(), reply.size());
            res = resp::parse(conn.inputReadableView());
        }
        if (res.status == resp::ParseStatus::Error)
            on_close(conn); // 格式错误理应关闭连接

        // 已经将命令结果追加进写缓冲区
        conn.send();
        // 写失败关闭连接，已经将文件描述符添加进 pending_close

        // 如果一次没有写完，那么就将 监听 读事件 修改为监听 读|写事件
        if (conn.outputReadableView().size() != 0)
            conn.getChannel().setEvents(EPOLLIN | EPOLLOUT);
        loop.updateChannel(&conn.getChannel());
    };

    // 设置listen_fd的读事件回调及其回调函数
    acpt.setNewConnectionCb([&](int client_fd) { // 注册client_fd的可读事件
        std::unique_ptr<Connection> client_conn = std::make_unique<Connection>(client_fd, EPOLLIN);
        client_conn->setOnMessage(on_msg);
        client_conn->setOnClose(on_close);
        client_conn->setModEpoll_EPOLLIN(
            [&loop](Connection &conn)
            {
                conn.getChannel().setEvents(EPOLLIN);
                loop.updateChannel(&conn.getChannel());
            });
        loop.updateChannel(&client_conn->getChannel());
        con_map.emplace(client_fd, std::move(client_conn));
    });

    // drop 进行资源释放
    auto cleanupClosed = [&pending_close, &con_map, &loop]()
    {
        for (auto &pc : pending_close)
        {
            // 检查con_map连接表内是不是存在该连接
            auto pos = con_map.find(pc);
            if (pos == con_map.end())
                continue;
            // 从红黑树上移除
            loop.removeChannel(&pos->second->getChannel());
            con_map.erase(pos);
        }
        pending_close.clear();
    };
    loop.setAfterLoopCb(cleanupClosed);

    loop.loop();

    // 进程退出时 OS 回收
    con_map.clear();
    return 0;
}