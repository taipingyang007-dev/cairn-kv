# D6b 下午：Poller / EventLoop / Acceptor（2026-09-11 定稿，设计冻结）

> 上午已达成：`if (fd == server_fd)` 消失，29/29，commit `92f4bbe`。
> 下午三步，**每步单独编译 + 单独跑 29 项验收**。做完一步算一步，不必今天全做完。

## 每个类对应的"麻烦"

| 麻烦 | 类 |
|---|---|
| `epoll_create1`/`epoll_ctl`/`epoll_wait` 散在 main 三处，换 io_uring 要全找一遍 | Poller + EpollPoller |
| `while(1)` 里混着"等事件"和"收尾清理"，没地方塞定时器（D10 定期删除要用） | EventLoop |
| `server_fd` 有 7 处错误清理要 close；`socket/bind/listen` 和 `accept` 被拆在两个地方 | Acceptor（+ Socket） |

---

## P1 — Poller（接口）+ EpollPoller（实现）

**职责**：把"等事件"这件事关进一个类。**全项目只有 `EpollPoller.cpp` 认识 `epoll_*`。**

三个动作，签名自己定：

```
updateChannel(Channel*)     注册 / 修改关注的事件   ← 现在的 epoll_ctl ADD / MOD
removeChannel(Channel*)     摘除                   ← 现在的 epoll_ctl DEL
poll(timeout) → 就绪的 Channel* 列表               ← 现在的 epoll_wait
```

**搬走**：`main.cpp` 的 `epoll_create1`（65–71）、`epoll_event temp_event, ep[1024]`（73）、两处 `epoll_ctl ADD`、`drop` 里的 `epoll_ctl DEL`、`epoll_wait`。

**要自己决定的**：
- `Poller` 是纯虚基类还是直接一个类？（提示：有第二个实现之前，抽象值不值？）
- `poll` 怎么把就绪的 Channel 交出去——返回 vector？还是传一个 out 参数？
- `setRevents` 该由谁调用？

**约束**：`EpollPoller` 拥有 `epoll_fd`，析构时 close。`main.cpp` 里不许再出现 `epoll_` 开头的东西。

**验收**：`grep -n "epoll_" src/main.cpp` → 0 条；29/29。

---

## P2 — EventLoop

**职责**：循环本身。

```
loop()  = while(1) { poller_->poll() → 逐个 setRevents + handleEvent → 执行 pending 任务 }
```

**已定**：收尾用 **B 方案 —— `queuePending(std::function<void()>)`**
任何时候可以往队列里塞任务，**每轮事件处理完之后**统一执行、然后清空。

为什么是 B：它是 `pending_close` 的一般化。今天塞的是"关连接"，D10 塞的是定时任务，以后多线程塞的是"跨线程调用"。**一个口子顶三个用途。**

`EventLoop` 持有 `Poller`，**不认识 `Connection`**（否则就不通用了）。

**搬走**：`main.cpp` 的 `while(1)`（162–186）。

**要自己决定的**：`pending_close`（`vector<int>`）还留不留？还是直接往 `queuePending` 里塞一个捕获了 fd 的 lambda？

**验收**：`main.cpp` 里没有 `while`、没有 `for` 循环；29/29。

---

## P3 — Acceptor（+ Socket）

**职责**：监听 socket 的完整生命周期——`socket` / `SO_REUSEADDR` / `bind` / `listen` / `accept`。

**搬走**：`main.cpp` 的 26–62（建 socket 那一串）、75–76（listen_channel）、131–160（accept 那段读回调）。

`accept` 成功后调用一个由上层填的回调：`new_connection_cb_(client_fd)`——**Acceptor 不认识 `Connection`**，跟 `Channel` 不认识 epoll 是同一条原则。

**这一步顺手抽 `Socket`**：此时 `Acceptor` 和 `Connection` 都持有必须 close 的 fd，**抽象有两个用户了**。收益：`main` 里 7 处 `close(server_fd)` 全部消失（析构自动关）。

**验收**：`main()` 剩 30 行左右，全是业务接线，没有一行 syscall；29/29。

---

## 三条不许违反的约束

1. **只有 `EpollPoller.cpp` 认识 `epoll_*`**
2. **`EventLoop` 不认识 `Connection`；`Acceptor` 不认识 `Connection`**
3. **每步单独跑 `rtk bash tests/acceptance.sh`，绿了才进下一步**

## 今天不做

上午记的遗留（`send` 契约注释、EPOLLHUP/ERR、EINTR 误关、`handleRead` 提成员函数、非阻塞 + EPOLLOUT）一律不动，见 `D6b-剩余步骤.md`。
