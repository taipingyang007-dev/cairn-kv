# 清理清单（2026-09-12 定稿，2026-09-14 更新）

> **2026-09-14 排期调整**：简历定稿从 9/19 推到 **9/22–9/25**。
> 优先级重排——**RocksDB 持久化（D11/D12）是简历标题里的词，优先于 D8 命令集 / D9-D10 过期删除**。

> 所有"知道有问题但今天不改"的东西，集中在这一份。改完一条划掉一条。
> 分三档：**D6c 必做**（非阻塞那步绕不开）／**随手改**（十分钟以内）／**等触发条件**。

---

## D6c 必做（非阻塞写那一步一起做）

- [ ] `Connection::send` 返回 `bool` → 改成"实际写出了多少字节"。
      现在非阻塞下只写出一半时，`outputRetrieve(全部)` 会**丢数据**
- [x] ~~`Connection.cpp` 的 `write_cb_`：`send` 完不 `retrieve`~~ **2026-09-12 做完**：`send()` 改成无参，自己从 `output_` 取、自己 `outputRetrieve(sent)`，两条调用路径一起修好
- [ ] EPOLLOUT 的挂/摘逻辑：写不完才 `EPOLL_CTL_MOD` 挂上，写完立刻摘掉
      （**EPOLLOUT 常驻 = busy loop，epoll_wait 永不阻塞，CPU 100%**）
- [ ] `read_cb_` 的 `EINTR` 分支现在会误关连接 → 改成什么都不做
- [ ] `Channel::handleEvent` 没处理 `EPOLLHUP` / `EPOLLERR`
      （内核不管你注没注册都会给；只给 HUP 不给 IN 时，现在两个分支都不进 → 连接永不清理）

## 随手改（十分钟以内，2026-09-12 review 出来的）

- [ ] **A** 删 `Socket::Socket()` 默认构造。现在没人用了，留着是陷阱：
      `Socket s;` 会悄悄发起一次 `socket()` 系统调用，读头文件看不出来
- [ ] **B** `Accept::createListenFd` 改成 `static`（或匿名 namespace 的自由函数），并移出 public。
      **它在 `socket_` 构造前被调用，加 `static` 后编译器会直接拒绝访问成员——根治 9/12 踩的那个 bug**
- [ ] **C** 删死代码：`Accept::getFd()`、`Connection::handleChannel()`（main 里都没人调了）
- [ ] **D** `X(X &) = delete` → `X(const X &) = delete`，三处：`Socket` / `Accept` / `EventLoop`
- [ ] **E** 删参数上的顶层 const：`setPollState(const PollState ps)` → `(PollState ps)`。
      按值传的东西，调用方给的是副本，const 不保护任何东西
- [ ] **F** 命名：`Accept` → `Acceptor`（类名用名词）；`setQueuePending` → `setAfterLoopCb`
      （它是"覆盖一个钩子"，不是"往队列里塞"）；`Channel::getPollState()` 返回类型去掉 `Channel::` 限定
- [ ] **G** 头文件里给参数起名字：`setNewConnectionCb(std::function<void(int)> cb)`
- [ ] `Accept` 的读回调应由 **Accept 自己在构造函数里挂**（`channel_.setReadCb([this]{ handleRead(); })`），
      main 只负责 `loop.updateChannel(&acpt.getChannel())`。
      **判据："事件来了干什么"只有 Accept 知道；"注册到哪个 loop"只有 main 知道**
- [ ] **加一个 Debug 构建**：`rtk cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug`
      **默认 Release 带 `-DNDEBUG`，你写的 assert 到今天一次都没跑过**（note 31 有实测）。
      验收脚本应该跑 Debug 版，平时压测才跑 Release
- [ ] `tests/acceptance.sh` 加一条防呆：检查 `src/` 下所有 `.cpp` 都在 `CMakeLists.txt` 里
      （已经漏了 4 次：Channel / EpollPoller / EventLoop / Socket）

## 等触发条件（现在不做，条件到了再做）

| 事情 | 触发条件 |
|---|---|
| `TcpServer` 类（装 `con_map`） | **多 Reactor**。触发点是"新连接分给哪个 EventLoop"这个决策没地方放。现在 `con_map` 只有两处操作，包一层没有收益 |
| `EventLoop` 的收尾从 A 方案（固定钩子）改 B 方案（任务队列 `queueInLoop`） | **多线程**。B 的第二个用户是"跨线程塞任务"，现在还不存在 |
| `main.cpp` 里的 `EPOLLIN` 宏 | 真要换 io_uring 时。muduo 的做法是在 `Channel` 里定义自己的 `kReadEvent` |
| `Connection` 的 Buffer 转发函数 → 暴露 `Buffer& input()/output()`（muduo 式） | **出现第二个操作连接缓冲区的模块时**（如 Raft 日志复制）。~~原写"业务搬进 Connection"是错的，业务永远不进 Connection~~ |
| 随手改：删 `inputAppend`（死代码），`outputRetrieve` 收 private | 任意时候，2 分钟。6 个转发 → 4 个，剩下全有人用 |
| `RespParser` 返回 `Error` 时坏数据没清出 `input_` | 反正要关连接，影响小 |
| **日志系统**（等级 / 时间戳 / 节点 id） | **Raft 多节点调试时**。现在 5 处 `fprintf(stderr)` 够用；自己写日志库是简历填充物，面试官跳过 |
| `--dir` 命令行参数（数据目录，默认 `data`） | 做 D8 命令集时顺手。现在相对路径从仓库根跑即可 |
| `loop.updateChannel()` 失败没处理 | 会留"在 con_map 里但不在 epoll 里"的僵尸连接。等真遇到再说 |

## 面试准备（非代码，RocksDB 落地后排）

- [ ] **Redis + MySQL 缓存一致性**这一族：先删缓存 vs 先更库 / 延迟双删 / 订阅 binlog、穿透（布隆 / 空值）、击穿（互斥锁 / 逻辑过期）、雪崩（随机 TTL / 多级）、热 key。
      **知识题不是项目题**——两三小时读透能答。**不进 cairn**（cairn 是被缓存的一方，不是用缓存的一方）；要落项目去 bridgeIM 加缓存层。
- [ ] RocksDB 内部（用了就会被问）：LSM-tree、memtable → SSTable、WAL、compaction（leveled vs tiered）、读放大 / 写放大 / 空间放大的三角。
