# D6b 剩余步骤（2026-09-11 重排，设计已冻结）

> **唯一目标**：`main.cpp` 的 for 循环体里，**一个 if 都不剩**。
> 每步只改一处、只讲一个知识点、单独验收。不许跨步。

## 已完成

- `Channel`：fd_/events_/revents_ + 读写回调，`handleEvent()` 按 revents_ 分发。Rule of Zero，移动 = delete
- `Connection`：内嵌 channel_，构造时挂 read_cb_；两层回调 on_message_/on_close_
- **fd 只存一份**（`Channel::fd_`）；`Connection` 仍是唯一所有者（只有它 close）
- `listen_channel` 已建、`server_fd` 已改用 `data.ptr` 注册
- `on_msg` / `on_close` / `pending_close` 三个 lambda 已写好

---

## S1 — 修 accept 段里的两行（就地，不搬家）

| | |
|---|---|
| **改哪** | `main.cpp` accept 那一段（约 161–173） |
| **做什么** | ① 注册进 epoll 的必须是 `&conn->getChannel()`（堆上 Connection 内嵌的那个）<br>② `con_map.emplace(client_fd, std::move(client_conn))`<br>③ `setOnMessage` / `setOnClose` 挪到 `epoll_ctl` **之前** |
| **知识点** | `unique_ptr` 的移动：move 之后局部指针变空，但**堆上的对象一动没动**——所以 move 之前取的 `&channel` 依然有效 |
| **为什么③** | `epoll_ctl` 一执行，事件随时可能到。回调没挂上就注册 = 开门营业但店里没人 |
| **验收** | 这一段之内无编译错误（整体仍过不了） |

## S2 — 把 accept 整段搬进 `listen_channel.setReadCb`

| | |
|---|---|
| **做什么** | 纯搬家，一个字不改内容 |
| **知识点** | lambda 捕获：这里能用 `[&]`，因为捕获的东西全在 `main` 栈上、活得比 lambda 久 |
| **⚠️ 坑** | accept 段里有 3 个 `continue`。搬进 lambda 后**没有循环可以 continue** → 编译错。要改成 `return` |
| **验收** | 编译错误只剩 for 循环那边 |

## S3 — for 循环体删到三行

```
Channel* ch = static_cast<Channel*>(ep[i].data.ptr);
ch->setRevents(ep[i].events);
ch->handleEvent();
```

| | |
|---|---|
| **知识点** | `epoll_event` 只有两个字段：`events`（**实际发生**的）和 `data`（你存什么它还什么）。**里面没有 fd** |
| **验收** | `grep -n "data.fd\|if (channel" src/main.cpp` → 一条都没有 |

## S4 — 补 pending_close 清理

| | |
|---|---|
| **改哪** | for 循环**结束之后**、`while` 里面 |
| **做什么** | 遍历 `pending_close`，逐个 `drop(fd)`，然后 `clear()` |
| **知识点** | 延迟销毁：回调里只**登记**，栈上没人了才**执行**。先 `EPOLL_CTL_DEL` 再 `erase` |
| **顺带** | 同一个 fd 可能重复登记（Error + send 失败）→ drop 前看 `con_map` 里还在不在 |
| **验收** | `rtk cmake --build build` 编译过、零警告 |

## S5 — 跑验收

```
rtk bash tests/acceptance.sh     # 29/29
```

## S6 — 收工

commit + 更新 `docs/PROGRESS.md` 当前状态 + 把下面的遗留抄进待办

---

## 三条不许违反的约束

1. `data.fd` 和 `data.ptr` 不许混用，全项目统一 `ptr`
2. 只有 `Connection` 能 close 客户端 fd（accept / setsockopt / epoll_ctl ADD 三处失败除外——那时还没建 Connection）
3. 销毁必须延迟到 for 循环之后；回调里只往 `pending_close` 塞

---

## 已知遗留（今天不做）

- `Connection.h` 缺 `send` 的契约注释："写失败时内部已触发 on_close_，调用方不要再关一次"
- `Connection.cpp:42` 写回调 send 完不 retrieve → 一旦挂上 EPOLLOUT 会重复发送。D6c 连非阻塞一起改
- `send` 的返回值该改成"发出去了多少字节"，现在的 bool 在非阻塞下会丢数据
- `read_cb_` 的 `EINTR` 分支现在会误关连接 → 改成什么都不做
- `RespParser` 返回 `Error` 时，坏数据留在 `input_` 里没清（反正要关连接，影响小）
- `handleChannel()` / 六个转发函数在 S3 之后成死代码 → 择机收
- 抽 `Socket` 类：等 `server_fd` 搬进 `Acceptor`（下午）再做，能顺带干掉 main 里 7 处 `close(server_fd)`
- `fd` 只留 `Channel::fd_` 已完成 ✅
