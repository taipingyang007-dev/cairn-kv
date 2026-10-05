# cairn 学习笔记 · 索引

> **编号是全局唯一的身份，跨文件不重编。** `PROGRESS.md` 里写的"note 条目 N"，在这里查它在哪个文件。
> 🟢 = 盖住笔记复述通过　🟡 = 讲过但没复述，不算掌握（欠账清单在 [PROGRESS.md](../PROGRESS.md) 的「待答的题」）
>
> **三份文档的分工**：[CHECKLIST.md](../CHECKLIST.md) = 写代码**前**念（防，10 条上限）；[DEBUG.md](../DEBUG.md) = 出问题**后**查（治，症状→命令）；**本目录 = 原理归档**（学，要复述过才 🟢）。
> **这份是「出事了去查原理」。「出事前挡住」的那份是 [CHECKLIST.md](../CHECKLIST.md)** —— 硬上限 10 条，写完一个函数念一遍。两份的分工别混：笔记可以有三十条，清单不行。

| 文件 | 主题 | 条目数 |
|---|---|---|
| [net.md](net.md) | 网络与 I/O —— socket / TCP / epoll / Buffer / Nagle / Reactor | 18 |
| [proto.md](proto.md) | 协议与命令 —— RESP 编解码 / 命令处理 | 4 |
| [cpp.md](cpp.md) | C++ 语言与工程 —— 编译链接 / 生命周期 / 传参 / 代码组织 / 调试工具 | 15 |
| [perf.md](perf.md) | 性能与压测 —— 测量方法论 / 瓶颈定位 | 1 |
| [store.md](store.md) | 存储 —— MemStore / IStore / RocksDB / LRU | 1 |
| [tools.md](tools.md) | 工具与踩坑 —— 命令速查 + 环境坑（**不用复述**） | — |

---

## 全部条目

| # | 标题 | 状态 | 日期 | 在哪 |
|---|---|---|---|---|
| 1 | 客户端凭什么认你 —— 协议是唯一契约 | 🟢 | 2026-09-02 | [proto.md](proto.md) |
| 2 | fd 是什么 —— 一个数组下标 | 🟢 | 2026-09-02 | [net.md](net.md) |
| 3 | 字节序与 htons —— 数值 = 字节 + 解读规则 | 🟢 | 2026-09-02 | [net.md](net.md) |
| 4 | bind —— 声明所有权，仅此而已 | 🟢 | 2026-09-02 | [net.md](net.md) |
| 5 | listen 与 accept 队列 —— 握手是内核干的 | 🟢 | 2026-09-02 | [net.md](net.md) |
| 6 | 监听 socket vs 已连接 socket —— 为什么 accept 返回新 fd | 🟢 | 2026-09-02 | [net.md](net.md) |
| 7 | read/write 返回值 —— 三分支与短读 | 🟢 | 2026-09-02 | [net.md](net.md) |
| 8 | TIME_WAIT 与 SO_REUSEADDR —— 放行的是 bind，不是缩短等待 | 🟢 | 2026-09-02 | [net.md](net.md) |
| 9 | COMMAND DOCS 卡死之谜 —— v0 唯一的一次 read 被握手抢走 | 🟢 | 2026-09-03 | [proto.md](proto.md) |
| 10 | write 与内核发送缓冲区 —— 返回值是"内核收下了几个" | 🟢 | 2026-09-03 | [net.md](net.md) |
| 11 | epoll 在内核里是什么 —— 值班室模型 | 🟢 | 2026-09-04 | [net.md](net.md) |
| 12 | sizeof 陷阱族 + 盲写毕业考存档 | 🟢 | 2026-09-04 | [cpp.md](cpp.md) |
| 13 | Buffer —— 字节的候车室，双索引 + 延迟挪动 | 🟢 | 2026-09-05 | [net.md](net.md) |
| 14 | 生命周期与传参 —— 悬空指针那一族 | 🟡 | 2026-09-06 | [cpp.md](cpp.md) |
| 15 | epoll 事件循环的实战陷阱 | 🟡 | 2026-09-06 | [net.md](net.md) |
| 16 | RESP 回复类型与 redis-cli 的显示 | 🟢 | 2026-09-06 | [proto.md](proto.md) |
| 17 | 编译与链接 —— 四个阶段，两个字母 | 🟢 | 2026-09-07 | [cpp.md](cpp.md) |
| 18 | 函数指针 vs std::function | 🟢 | 2026-09-14 | [cpp.md](cpp.md) |
| 19 | 代码怎么分家：命名空间 / 头文件 / 类 | 🟡 | 2026-09-07 | [cpp.md](cpp.md) |
| 20 | Nagle 算法 × 延迟 ACK —— pipelining 下吞吐反而下降 | 🟢 | 2026-09-16（复习重标，见条目内记录） | [net.md](net.md) |
| 21 | 压测方法论 | 🟢 | 2026-09-09 | [perf.md](perf.md) |
| 22 | 命令分发表：把 if-else 变成数据结构 | 🟢 | 2026-09-09 | [proto.md](proto.md) |
| 23 | Reactor 五个角色（设计已通，代码待写） | 🟢 | 2026-09-11 | [net.md](net.md) |
| 24 | 特殊成员函数的生成规则（Rule of Five 的由来） | 🟡 | 2026-09-11 | [cpp.md](cpp.md) |
| 25 | 成员初始化的两个坑（声明顺序 / 参数先于自己求值） | 🟢 | 2026-09-12 | [cpp.md](cpp.md) |
| 26 | most vexing parse | 🟡 | 2026-09-11 | [cpp.md](cpp.md) |
| 27 | 继承 vs 组合（is-a / has-a） | 🟡 | 2026-09-12 | [cpp.md](cpp.md) |
| 28 | 循环 include 的报错形态 | 🟢 | 2026-09-12 | [cpp.md](cpp.md) |
| 29 | 边界：依赖向下，控制向上 | 🟡 | 2026-09-12 | [net.md](net.md) |
| 30 | 决定"东西该放哪个类"的三个问题 | 🟡 | 2026-09-12 | [net.md](net.md) |
| 31 | `assert` 在 Release 下根本不存在（-DNDEBUG） | 🟢 | 2026-09-12 | [cpp.md](cpp.md) |
| 32 | 无符号减法下溢 —— `0 - 6` 不是 `-6` | 🟢 | 2026-09-12 | [cpp.md](cpp.md) |
| 33 | 触发者 ≠ 拥有者（Channel 不拥有 Buffer） | 🟢 | 2026-09-12 | [net.md](net.md) |
| 34 | 定一个函数签名的三问法（函数粒度，对应 note 30 的模块粒度） | 🟡 | 2026-09-12 | [cpp.md](cpp.md) |
| 35 | 栈帧 · ASan 读法 · 调试工具分层 · 内存 bug 的签名 | 🟢 | 2026-09-12 | [cpp.md](cpp.md) |
| 36 | EPOLLHUP / EPOLLERR 的真实行为（实测：TCP 的 HUP 总伴随 IN） | 🟡 | 2026-09-14 | [net.md](net.md) |
| 37 | 不在回调栈里删自己 —— 延迟释放（pending_close = Redis freeClientAsync） | 🟡 | 2026-09-14 | [net.md](net.md) |
| 38 | char / char* / char[] / string —— 字节、地址、长度 | 🟡 | 2026-09-15 | [cpp.md](cpp.md) |
| 39 | RocksDB 的读写路径 —— LSM 的全部（WAL / memtable / SST） | 🟢 | 2026-09-17（盖笔记复述过） | [store.md](store.md) |
| 40 | LSM 的分层与 compaction（L0 为何特殊 / leveled vs tiered / 三个放大🔴） | 🟡 | 2026-09-17 | [store.md](store.md) |
| 41 | Bloom Filter —— 解决"查不存在的 key 最慢" | 🟡 | 2026-09-17 | [store.md](store.md) |

---

## 用法（当初定的规矩，别改）

1. **每讲透一块，落一条。** 只记讲透过的东西——没讲到的不预先写，写了就变成喂答案。
2. **复述时盖住文件**，凭记忆讲。讲通了才标 🟢 + 日期。只"听过"记 🟡。
3. **和 `PROGRESS.md` 的分工**：那边的「待答的题」是**欠账清单**（队列），这里是**已学会的归档**。一道题 🟢 之后，从那边划掉、在这边落条目。
4. **编号只增不改。** 条目搬家可以，重编号不行——所有引用都会断。
