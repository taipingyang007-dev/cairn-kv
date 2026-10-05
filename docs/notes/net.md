# cairn 笔记 · 网络与 I/O

> socket / TCP / epoll / Buffer / Nagle / Reactor

> **用法**：每讲透一块，在这里落一条。复述时**盖住这个文件**，凭记忆讲，讲通了在条目上标 🟢 + 日期。
> **编号是全局唯一的身份，跨文件不重编** —— `PROGRESS.md` 里的"note 条目 N"永远指同一条。
> 索引在 [README.md](README.md)。

---

### 2. fd 是什么 —— 一个数组下标 🟢 2026-09-02

**一句话**：fd 就是当前进程「文件描述符表」（一个指针数组）的**下标**。内核"找到你的 socket" = 拿当前进程 + 下标查表、取格子里的指针，O(1)。

- **分配规则**：总是给**当前最小的空格**。0/1/2 出生即被 stdin/stdout/stderr 占用，所以第一个 socket 拿到 3，`accept` 回来的连接拿到 4。
- **"一切皆文件"的落地**：格子里指向什么都行（普通文件、管道、终端、socket）。每个 `struct file` 带一组函数指针（`file_operations`），`read()` 只管查表分发，不关心背后是磁盘还是网卡——所以同一个 `read()` 能读网络数据。
- **精度脚注**：表是进程私有的，但**格子里的指针可以指向同一个内核对象**——`fork()` 继承整张表后，父子的 fd=3 指向同一个 socket（"下标私有，对象可共享"）。追问"父子进程共享文件偏移吗"根子在此。
- **项目连线**：① 循环 accept 的本质 = 不断领新下标（毛病 1）② 忘 `close` = fd 泄漏，表满（`ulimit -n` 默认 1024）后 `accept` 报 `EMFILE` ③ epoll 监视的就是一筐 fd（毛病 4）。

**我的复述**（2026-09-02，原话存档）：每个进程在内核有一张文件描述符表，其实是数组，3 是下标，数组指向一个个文件（普通文件/socket/管道），内核通过进程所属的表查表得到 socket。两个进程的 fd=3 不是同一个东西，因为表是进程独享的，不同的表找相同下标自然不同。

### 3. 字节序与 htons —— 数值 = 字节 + 解读规则 🟢 2026-09-02

**一句话**：内存里没有"数"，只有字节。数值是「字节 + 解读规则」的产物：同一串字节 `[EC][18]`，小端读 = 6380，大端读 = 60440。**字节从没变，变的是读法。**

- `htons(6380)`：6380 = `0x18EC` → 把两个字节对调成 `[18][EC]`。它是普通函数，**赋值当场算完**，不是"传输时的转换"（大端机器上是空操作）。
- **去掉 htons 会怎样**：不报错、正常启动，**默默听在 60440**。原因：`sin_port` 的 API 契约 = 这个字段的字节一律按网络序（大端）解读。`bind()` 时内核按大端读 `[EC][18]` → 在端口登记表记下 60440。**全程发生在本机内核里，一个包都没发。**
- **API 为什么这么设计**：`sockaddr_in` 是"线上格式的预装配"——以后每个进来的包，内核直接拿包头字节和登记的字节**比对**，不逐包转换。
- **为什么线上要统一字节序**：两端可能一台大端一台小端，线上字节不自带说明书 → **字节序也是契约的一部分**（同条目 1：协议是唯一契约）。
- **实验存档**（2026-09-02）：删 htons 重编译 → `ss -ltn` 看到 `0.0.0.0:60440`；`redis-cli -p 60440 PING` 能通。`0.0.0.0` = `INADDR_ANY`（本机所有网卡都接）。
- **`ss -ltn` 读法**：`-l` 只看 LISTEN / `-t` TCP / `-n` 显数字。一行一个监听 socket；`Local Address:Port` = 听在哪；`Peer` 的 `*` = 尚无对端；LISTEN 行的 `Recv-Q`/`Send-Q` 有特殊含义 → 见 backlog 条目（待写）。
- **这类 bug 的画像**：不崩、不报错，只是"听错了地方"——凡是"存的规则"和"读的规则"不一致都是这个画像。

**我的复述**（2026-09-02，原话存档）：错在 bind 读取 sin_port 和内存存储端口用的不是同一个规则——存的时候小端，bind 按大端读，所以绑定的端口成了 60440。

### 4. bind —— 声明所有权，仅此而已 🟢 2026-09-02

**一句话**：bind = 查全局端口登记表防冲突 + 登记「(协议, IP, 端口) → 这个 socket」的所有权。**不发一个包、不进入收客状态、不建任何队列。**

- **两张表的级别**：fd 表**每进程一张**（下标 → 内核对象指针，进程私有的名字）；端口登记表**全内核一张**（端口是全机共享资源，必须全局裁决谁占）。正因为端口表全局，bind 才可能失败。
- **端口被占** → 返回 -1，`errno = EADDRINUSE`（"Address already in use"）。`SO_REUSEADDR` 整个围绕这个错误 → 见条目（待写）。
- **跳过 bind 直接 listen** → 能跑不报错，内核自动分配随机临时端口（IP/协议在 `socket()` 时已定，缺的只是端口）；只是没人知道你的门牌。**bind 的意义 = 服务器需要固定门牌号，不是 listen 的前置仪式。**
- **bind 了还没 listen** → 连不上。收客状态和排队区是 listen 的事。正常系统客户端见 `Connection refused`；WSL mirrored 下表现为卡死。
- **谁需要被找到，谁才 bind**。客户端 = `socket()` + `connect()`，临时端口由内核在 connect 时自动分配——它无需被预先找到。
- **四步各做一件小事**：`socket()` 盖房拿钥匙 → `bind()` 挂门牌 → `listen()` 开门营业备排队区 → `accept()` 迎一位客人进门。

**我的复述**（2026-09-02，原话存档）：bind 先查端口登记表，被占则返回 -1 置 EADDRINUSE；fd 表进程内独立互不相通，端口登记表内核全局仅一张，(协议,ip,端口)→socket；跳过 bind 能跑但客户端不知道连哪；bind 后未 listen 连不上，真正可连接需要 listen。

### 5. listen 与 accept 队列 —— 握手是内核干的 🟢 2026-09-02

**一句话**：`listen` = 开门营业 + 建排队区；`accept` = 从队列取走一个**握手已完成**的连接，包装成新 fd。**三次握手全程由内核完成，accept 不参与握手。**

- **两个队列**：半连接队列（握手进行中的"请求"）→ 握手完成 → **全连接队列**（等被 accept 取走）。`backlog(511)` = **全连接队列的容量**。Redis 本尊也用 511。
- **"监听数量"错在哪**：不是能监听几个端口，也不是最多同时服务几个客户端——**被 accept 取走的连接已出队，不占名额**。只管"已握手、没被取走"的排队者。
- **金句**：连接建立成功 ≠ 服务器程序理你了。实验实证：v0 阻塞在第一个连接的 `read` 时，第二个 `connect` 秒成功、进队、无人取，卡在等回复直到超时。
- **三种遭遇对比**（面试必分清）：

  | 场景 | 内核行为 | 客户端体验 |
  |---|---|---|
  | 端口没人听 | 回 RST，明确说"不" | **立刻** refused（WSL mirrored 伪装成卡死） |
  | 队列有空位 | 握手瞬间完成，进队 | connect 秒成功，卡在等应用回复 |
  | 队列满（第 512 个） | **丢弃 SYN，一声不吭** | 以为丢包，重发 SYN，几十秒~分钟后才超时 |

  **拒绝 = 明确说"不"（立刻）；丢弃 = 装没听见（客户端自己慢慢死心）。** Linux 默认丢弃（`tcp_abort_on_overflow=0`），赌重传到达时队列已腾空。
- **`ss -ltn` 伏笔兑现**：LISTEN 行 `Send-Q` = backlog 生效值（511），`Recv-Q` = 此刻 accept 队列排队数。
- **项目连线**：v0 永远处理不了第二个连接，因为没有循环——这就是毛病 1 的问题陈述（我自己推出来的）。

**我的复述**（2026-09-02，原话存档）：backlog 是等待被 accept 取走的全连接队列长度；半连接是握手中的请求，全连接是握手完成的连接。溢出时服务器内核直接丢弃 SYN，客户端会重发 SYN。端口没人听（没 listen）则直接拒绝。

### 6. 监听 socket vs 已连接 socket —— 为什么 accept 返回新 fd 🟢 2026-09-02

**一句话**：监听 socket = "门 + 排队区"，无对端、无字节流；每条 TCP 连接由**四元组** `(本机IP:端口, 对端IP:端口)` 唯一标识，在内核里是**独立对象**（自己的收发缓冲区、序号、状态机）。read/write 必须指名一条字节流 → **每条连接一个 fd，不是品味，是必然**。

- **归谬体感**：若复用 server_fd——两个客户端连着时 `write(server_fd, "+PONG")` 发给谁？`read(server_fd)` 读谁？内核无从回答 → API 强制分离。
- **时间线**：握手完成 → 内核创建已连接 socket 对象（身份=四元组）→ 进全连接队列 → accept 取出、fd 表最小空格放指针 → 返回新下标。
- **client_fd 不是"客户端的 fd"**：它是这条连接**在本进程 fd 表里的名字**；对端进程有自己的名字（接条目 2：fd 是进程私有下标）。
- **100 个客户端 = 101 个 fd**（1 监听 + 100 已连接），且 **100 条连接共享本机 6380 端口**——端口不按连接消耗，四元组区分。（陷阱题："端口会不会被连接用光"）
- **分工铁律**：`server_fd` 只喂给 `accept`；`client_fd` 才喂给 `read`/`write`。
- **项目连线**：毛病 1 的循环 accept 每圈领一个新 client_fd；毛病 4 的 epoll 看守的就是一筐 client_fd。

**我的复述**（2026-09-02，原话存档）：每一个连接是一个字节流；server_fd 是监听 socket，绑定(协议,端口,IP)用于监听；连接成功后内核创建出四元组（本机IP+端口，对端IP+端口）标识的对象，client_fd 绑定上它。

### 7. read/write 返回值 —— 三分支与短读 🟢 2026-09-02

**决策表（毛病 1 循环的骨架）**：

| `n = read(...)` | 含义 | 动作 |
|---|---|---|
| `n > 0` | 读到 n 个字节 | 只处理这 n 个 |
| `n == 0` | 对端关闭（EOF） | `close(client_fd)`，服务下一位 |
| `-1` 且 `errno==EINTR` | 被**信号**打断，没读到东西 | **原地重试** |
| `-1` 其他 | 真出错（如 ECONNRESET 对端异常断开） | `perror` + `close`，下一位（服务器没有"重连"一说） |

- **read 的语义**：承诺"把**已到达**的字节给你，至多 N"；**不承诺**读满 N，更不承诺这是一条完整消息。**read 给的是字节流的一段，不是一条消息。**
- **TCP 精确表述（面试措辞）**：保证字节**都会到、按序到**（丢包重传）；不保证**一起到、按发送的块到**。"发 100 只读到 60" = 另 40 个还在路上 → **半包**。
- **半包的解法 = 划边界**：分隔符（`\r\n`）或长度头（`$6`）——RESP 两样都用。我自己从 RPC 项目经验（缓冲区 + 约定边界）推出了这个答案 → 这就是 Buffer 模块和 D4 增量解析的存在理由。
- **EINTR 为什么不算错误**：没有任何东西被破坏——数据还躺在内核缓冲区、连接完好、fd 完好，重试即恢复。它是"被打扰"，不是"失败"。
- **write 同理**：必须查返回值；契约允许**短写**（返回 < 请求量，发送缓冲区紧张时）→ 严谨做法循环写到写完。`-Wall -D_FORTIFY_SOURCE=2` 会对忽略返回值的代码报警（实测第 52 行中枪）。
- EAGAIN/EWOULDBLOCK：非阻塞 IO 专属，毛病 4（epoll）才登场。

**我的复述**（2026-09-02，原话存档，TCP 措辞已纠正）：>0 常规处理；=0 对端关闭应 close；-1 除 EINTR 外 perror+close，EINTR 应重新 read——因为 fd 和数据没有任何破坏、对端状态没问题，只是这一次没读成。read 承诺读 fd 内已到达的数据最大 1024，不承诺数据完整性。半包因为 TCP 分批到达（注意：TCP 保证到达，不保证一次性到达）。

### 8. TIME_WAIT 与 SO_REUSEADDR —— 放行的是 bind，不是缩短等待 🟢 2026-09-02

**一句话**：SO_REUSEADDR 允许 bind 一个**只被 TIME_WAIT 状态连接占着**的地址；那条 TIME_WAIT 连接本身**原封不动继续躺满 2MSL**。错误说法"避免等 2MSL"错在"避免"——什么都没被避免，只是 bind 不再被尸体挡路。

- **谁进 TIME_WAIT**：主动先 close 的一方。v0 里是服务器（write 完就 close）——所以重启才会被拦。
- **2MSL 怎么量**：MSL = 报文段在网络中的最大存活时间；2MSL = 最后的 ACK 传过去 + 对端重发的 FIN 传回来。
- **TIME_WAIT 保护两件事**：① 最后的 ACK 若丢失，对端重发 FIN 时我还在场、能重发 ACK，让对端体面关闭；② 防旧连接的延迟报文串进新连接（"幽灵数据"）——只有**相同四元组**的新连接才会撞上，所以才要占着这个地址等它们死绝。
- **挡路的具体机制**：重启后 `bind` 查端口登记表 → 端口被 TIME_WAIT 连接占着 → `EADDRINUSE`。
- **安全性**：放行的只是尸体，防串台的活儿 TIME_WAIT 照干，所以服务器加它是标准做法。
- **SO_REUSEPORT 🟢 2026-09-03**：允许**多个服务端进程各自的监听 socket**同时 bind+listen 同一 (IP,端口)（条件：全都设了 REUSEPORT 且同一用户），内核里就有 N 条独立 accept 队列，新连接按四元组哈希**分摊**——解决多进程并行 accept（nginx 多 worker），比 fork 共享一个监听 fd 少了抢队列和惊群。对比钩子：**ADDR = 接管死人的地址；PORT = 活人合租一扇门**。
- **统一视角（解开"端口只有一个怎么复用"的困惑）**：端口登记表的结构是「一个键 → 一串 socket」，"一个端口一个人占"**不是表的结构限制，是 bind 里的检查规则**。REUSEADDR 和 REUSEPORT 是同一条规则的两个豁免条款：前者豁免"只剩尸体"的情况，后者豁免"活人都同意合租"的情况。
- **与 cairn 的关系**：单 Reactor 用不上；但面试追问"单线程吃不满多核怎么办"时，REUSEPORT 多进程是标准答案之一（Phase 2 对比实验候选）。

**我的复述（REUSEPORT，2026-09-03，原话存档）**：两者都是针对端口登记表的复用。REUSEADDR 针对表内处于 TIME_WAIT 的，可以再次 bind；REUSEPORT 是表内已有监听 socket 时（都设了 REUSEPORT 且同用户）仍可再绑定一个监听 socket。

**我的复述**（2026-09-02，原话存档）：SO_REUSEADDR 允许 bind 一个只被 TIME_WAIT 占着的地址，那个 TIME_WAIT 连接本身会继续等待 2MSL。TIME_WAIT 一是确保对端真实收到 ACK 让对端关闭释放资源，二是防幽灵数据——避免上次连接的数据在新连接到达，只有同一个四元组才会出现。

### 10. write 与内核发送缓冲区 —— 返回值是"内核收下了几个" 🟢 2026-09-03

**一句话**：`write` 不是"发到网线上"，是"把字节交给内核的**发送缓冲区**"（快递站），内核之后自己往外送。**返回值 = 内核这次收下的字节数。**

**画面：每条已连接 socket 在内核里有一对缓冲区**

```
我的进程 ──write──▶ [发送缓冲区] ──内核按 TCP 节奏发──▶ 网线 ──▶ [对方的接收缓冲区] ──read──▶ 对方进程
我的进程 ◀──read─── [接收缓冲区] ◀───────────────────── 网线 ◀── [对方的发送缓冲区] ◀──write── 对方进程
```

- **接收缓冲区**：`read` 从这里**读走**——读走即删除（所以 EINTR 时数据不丢：根本没读走）；LT 模式下只要非空就持续触发 `epoll_wait`（条目 7、条目 9 的 3000 字节陷阱）。
- **发送缓冲区**：`write` 往这里放；内核发出去、**收到对方 ACK 后才腾出空位**。对方读得慢 → 对方接收缓冲区堆满 → 我的发送缓冲区跟着堆满 → `write` 放不进去。

**三种返回值（条目 7 的镜像）**

| `n = write(fd, buf, 7)` | 含义 | 动作 |
|---|---|---|
| `n == 7` | 全收下 | 完事（99%） |
| `0 < n < 7`（如 3） | **短写**：只剩 3 个位置 | 剩下 4 个再送一趟：`write(fd, buf + 3, 4)` |
| `-1` | 看 errno | `EINTR` 一个字节没交出去就被打断 → 重试；`EPIPE`/`ECONNRESET` 对端已关 → close；`EAGAIN` 仅非阻塞（毛病 4 展柜） |

- **短写何时发生**：发送缓冲区快满，或写到一半被信号打断。阻塞 socket 发 7 字节几乎碰不到——**代码对"几乎"负责，不对"一定"负责**。
- **忽略短写为什么致命**：客户端收到 `+PO`，下一条回复接上去变成 `+PO+PONG\r\n`——**整条连接的协议流从此错位**。
- **循环规则**：`sent = 0`；只要 `sent < len`，就 `write(fd, buf + sent, len - sent)`；返回 n → `sent += n`；-1 且 EINTR 重来；-1 其他返回失败。

  | 轮次 | sent | 调用 | 返回 | 之后 |
  |---|---|---|---|---|
  | 1 | 0 | `write(buf+0, 7)` | 3 | 3 |
  | 2 | 3 | `write(buf+3, 4)` | 4 | 7 = len，结束 |

- **SIGPIPE 陷阱**：往对方已关闭的连接 write，内核除了返回 EPIPE 还给进程发 `SIGPIPE`，**默认动作是杀死进程**——客户端 Ctrl+C 走了、服务器正好在回它，整个服务器就没了。服务器开头一律 `signal(SIGPIPE, SIG_IGN)`。
- **阻塞 vs 非阻塞预告**：阻塞模式下缓冲区满了 `write` 会等；非阻塞返回 `EAGAIN`，写不完的部分要自己存起来、等 `EPOLLOUT` 再写——这就是设计文档里 `Buffer` / `Connection` 要有**输出缓冲区**的原因（毛病 4 收尾时做）。
- **我踩过的 4 个实现坑（2026-09-03，第一版手写循环）**：① 判断了 `sent == -1` 而返回值在 `t_sent` 里——错误永远测不到，`sent += -1` 倒着走；② 长度算对了但起点没偏移，短写后从头重发；③ `sizeof(const char*)` 是指针大小 8，"+PONG\r\n" 恰好也是 8 才没炸——要用 `const char buf[]` 数组；④ close 之后没 `break`，下一圈往死 fd 上写，而且这个 fd 号可能已被新连接复用。

**我的复述**（2026-09-03）：以代码为准——main.cpp 里的 write 循环，四个坑修正后（`t_sent` 判错、`+ sent` 偏移、数组、`break`）通过 review。

### 11. epoll 在内核里是什么 —— 值班室模型 🟢 2026-09-04（讲三遍才过；前两遍败在用词讲，第三遍用"值班室 + 你自己的三个函数"才通）

**先安心：不用先学红黑树。** 这里红黑树只扮演"一张查名字很快的名单"，它内部怎么旋转怎么染色是数据结构课的事，与理解 epoll 无关。

**画面：epoll 实例 = 内核里的一间值班室**

```
epoll_fd ──(条目 2：fd 表里的一格)──▶ 【值班室】
                                        ├─ 墙上的名单（兴趣表，红黑树，键=fd）  ← epoll_ctl ADD/DEL 改名单
                                        └─ 门口的队伍（就绪链表）               ← epoll_wait 只看这里
```

- **`epoll_fd` 是什么**：又一个 fd——你 fd 表里的一格，指向内核里这间值班室（条目 2 的话："fd 是下标，格子里的指针指向内核对象"）。
- **两样东西**：① **兴趣表**（红黑树）——"我在盯着哪些 fd"，`epoll_ctl` 增删改；② **就绪链表**——"此刻谁有事"，`epoll_wait` 把它拷进你的 `ep` 数组就返回，队伍空就阻塞。
- **ADD 时的第三件事（魔法所在）**：内核在被监视的那个 socket 上挂一个**回调**——相当于给它发指令："你一有动静（数据到了/连接来了），自己去值班室门口排队"。于是 `epoll_wait` 不用挨个去问名单上的人，只看门口有谁。
- **为什么比 select/poll 快**：select/poll 每次调用都把**整张名单**挨个问一遍（监视 1 万个 fd 就问 1 万次，哪怕只有 3 个有事）；epoll 的代价只和**门口排队的人数**有关。这就是 C10K 问题的答案。
- **监听 socket 的"可读"**（条目 5 + 条目 6）：监听 socket 上没有字节流，它"可读" = **它的全连接队列非空**——有握手完成的连接在排队。此刻 `accept` 一定不阻塞，因为它就是从这条队列里取一个。
- **LT / ET**（条目 9 的 3000 字节陷阱）：LT = 只要接收缓冲区还有数据，每次 `epoll_wait` 都再报一次（门口的人没办完事不走）；ET = 只在"从无到有"那一刻报一次，剩下的你不读完就没人再提醒——所以 ET 必须非阻塞 + 循环读到 EAGAIN。

**三个函数各碰一样东西（记这张表就够）**：

| 代码 | 干什么 | 碰值班室里的什么 |
|---|---|---|
| `epoll_create1(0)` | 开一间值班室 | —— |
| `epoll_ctl(ADD/DEL)` | **改名单**：记上/划掉一个 fd | 兴趣表（红黑树） |
| `epoll_wait(ep, 1024, -1)` | **看队伍**：谁有事就拷进 `ep` | 就绪链表 |

- **为什么名单用红黑树**：ADD/DEL 要按 fd 快速查找、防重复；fd 值稀疏（3、7、1000…），开数组要么浪费要么放不下。**不必懂红黑树本身**——它只是"一张查名字很快的名单"。
- **链表 vs 数组的区分**：就绪队列在内核里是**链表**；代码里的 `ep[1024]` 是**用户态的接收容器**，`epoll_wait` 把就绪项拷进来。所以 1024 限制的是"一次最多拿几个"，不是"最多监视几个"——一次没拿完，下次 `epoll_wait` 立刻返回剩下的。
- **和 select/poll 比**：select/poll **每次调用**都把整个 fd 集合从用户态拷进内核、内核挨个问一遍、再拷回来；epoll 名单**常驻内核**，只拷就绪的那几个出来。代价只和"排队人数"有关，与"名单长度"无关。

**我的复述**（2026-09-04，原话存档）：epoll_ctl 碰的是内核内的监听文件描述符（名单），epoll_wait 碰的是就绪的，把前者送到后者的是回调函数——对应的文件描述符被监听到了就回调到就绪队列上。

### 13. Buffer —— 字节的候车室，双索引 + 延迟挪动 🟡 2026-09-05（Claude 写的，我讲解过、trace 过；盲写 ensureWritable 通过后转 🟢）

**身份**：字节的候车室。进来的字节在这等，凑够一条命令被领走，凑不够继续等。**Buffer 不认识 RESP**——找 `\r\n`、算 `$6` 是 RespParser 的事。具体理由：RESP 的 `$6\r\nfoobar\r\n` 里 `foobar` 可以含 `\r\n`，"找到 `\r\n` 就切"是错的。

**流水线位置**：`socket ──read──▶ Buffer ──peek──▶ Parser 看一眼 ──有完整命令──▶ 执行 + retrieve(n)；没有 ──▶ 留着等下次 read`

**四个操作**：`append(data, len)` 放进来 / `peek()` + `readable()` 只看不拿 / `retrieve(n)` 领走前 n 个。

**内部只有三样东西：一个 vector + 两个书签**（约定：**指"下一个"，不指"最后一个"**——左闭右开，同 `for (i=0; i<n)`、`begin/end`；空 = (0,0)，不需要 −1）

```
buf_:  [ 已取走 │ 可读 │ 空闲 ]     readable = writer − reader
       0     reader  writer size    writable = size − writer
                                    peek     = data + reader
```

**ensureWritable 三条路（灵魂）**：① 尾部空闲够 → 不动 ② 前面已取走 + 尾部空闲 够 → memmove 把可读搬到最前，reader=0 ③ 都不够 → resize 长大。"延迟"= 不是每次 retrieve 都挪，放不下那一刻才挪一次。
**retrieve 归零**：全取完时两个书签一起回 0，零成本回收。

**kInitialSize=8 的 trace（测试 4b）**：append PI → (0,2)；append NG\r\nPI → (0,8)；retrieve 6 → (6,8) 可读 PI；append 6：空闲 0 不够，已取走 6 + 0 ≥ 6 → memmove → (0,2) → 写入 → (0,8)；循环，容量永远 8。

**两个测试各堵一条回收路**：4a 每轮全取完 → 走归零；4b 永远留 2 字节半包 → 逼出 memmove。

**语法点**：
- 类里的常量一律 `static constexpr`（static：归类不归实例，`Buffer::kInitialSize` 无需对象；constexpr：编译期常量，C++17 起自动 inline，头文件写完即可）
- `vector.data()` = 内容首地址（交给 memcpy/read 等 C API）；`&vec` 是**记账本**地址，`sizeof(vec)` 是记账本大小——上午 `read(fd, &buffer, sizeof(buffer))` 两个 bug 的根子（条目 12 追加：容器的 sizeof 是账本不是内容）
- `data() + readerIndex_`：指针 + 整数 = 指针往后走 n 个元素，无转换；const 成员函数里 `.data()` 给 `const char*`，与返回类型严格一致
- `memmove` 允许源/目标重叠（同一数组内搬家），`memcpy` 不允许
- `assert` 用于**不变量**（`n <= readable()`），Release 消失无妨；**测试不能用 assert**（-DNDEBUG 下全消失 → 假通过），自写 `CHECK`。~~验收 5 实测：故意改错在 -O3 -DNDEBUG 下照样 abort~~ ⚠️ **2026-09-12 复核：这句错了。** 裸 `assert` 在 `-DNDEBUG` 下完全不触发（条目 31 有实测），那次 abort 大概率是 `_FORTIFY_SOURCE=2` 抓的越界写

**回收方案取舍**：挪动（1 个数，每次 retrieve 都 memmove）/ 环形（永不搬，但一条命令可能跨边界，解析器代码脏）/ **双索引延迟挪动（选它：多 5 行，面试经典 muduo Buffer）**。

**所有权补课（同日）**：`epoll_event.data` 是 union，内核对那 8 字节只保管不解释；`.ptr` 存 `Connection*` 负责"找回一个"，`unordered_map<int, unique_ptr<Connection>>` 负责"拥有全部"（erase 即析构 + 可遍历）。**堆决定放哪，所有者决定活多久**——`make_unique` 在 if 里 = 对象在堆上但 unique_ptr 在栈上，`}` 时一起死；unique_ptr 需要一个活得久的家。

### 15. epoll 事件循环的实战陷阱 🟡 2026-09-06

- **LT 模式下"收到事件但什么都不做" = 死循环，不是无害**。缓冲区还有数据 → 下次 `epoll_wait` 立刻又报 → 又跳过 → 100% CPU 空转，服务器活着但不干活。所以异常分支必须 `close_client`（把 fd 从 epoll 摘掉），不能只 `continue`。
- **`data` 是 union，存进去什么就得按什么读**。混用是致命的：注册时写 `data.fd = server_fd`（一个 int），读时 `((Connection*)data.ptr)->x` → 解引用地址 0x3 → 段错误。**方案 A：全部用 `data.fd`，客户端事件用 map 查 Connection；方案 B：全部用 `data.ptr`，监听 socket 存 nullptr 当哨兵。选一个，不能混。**
- **`&con_map[fd]` 的类型是 `unique_ptr<Connection>*`，不是 `Connection*`** —— 要 `.get()`。`void*` 接受任何指针不做检查，编译器不报错。
- **`erase` 与 `close` 的顺序**：`con_map.erase(fd)` 会析构 Connection，之后再 `conn->client_fd` 就是 use-after-free。解法不是"记住顺序"，是抽个 lambda 把值先复制进去：`auto drop = [&](int fd){ con_map.erase(fd); close_client(epoll_fd, fd); };` —— **参数按值传，进入函数前取值已完成，怎么调都安全**。lambda 是普通局部变量，作用域从定义处到 `}`，要定义在所有调用点之前。
- **`operator[]` 的坑**：key 不存在时**默默插入一个空条目**。防御用 `find()`（一次查找同时得到"在不在"和"是哪个"），比 `contains` + `[]`（查两遍）好。
- **`assert` + 兜底一起写**：`assert` 在 Debug 大声崩溃便于定位，Release 消失；下面的 `if` 兜底保命。**开发期要吵闹，生产期要坚韧**——服务器代码的常见做法。
- **`undefined reference` = 链接错误不是编译错误**：头文件给了声明所以编译过，但对应 `.cpp` 没进构建。CMake 里 `add_executable` 要列全所有源文件。

### 20. Nagle 算法 × 延迟 ACK —— pipelining 下吞吐反而下降 🟢 2026-09-16

> **2026-09-16 复习记录**：9/9 标 🟢 时只复述了时间线，**规则（MSS 边界）、三条件、分层都没复述到**。
> 判据教训：**复述「结论」（40ms / 三条件）不算通，复述「分层」才算。** 结论会忘，分层不会。
>
> ⚠️ **这条 🟢 的来历**：2026-09-16 第一轮 5 问答错 3（规则全错 / 以为只卡一次 / ⑤ 理由反了），
> 补讲后第二轮规则答全、tcpdump 实验预测命中（猜 2 个报文段，实测 2 个）。
> **但没有做盖住笔记的完整复述** —— 本人判断「结合 Redis 机制和 TCP 机制之后理解了」，要求标 🟢 并约定下次回顾重考。
> **下次回顾必须重考这三问**：① Nagle 扣住的两个条件 ② 为什么每一批都卡 40ms 而不是只卡一次 ③ 批量写和 Nagle 自己攒，字节数一样为什么效果不同。

#### 0. 根 —— 报文段 ≠ 请求（2026-09-16 补，真正卡住的地方）

```
应用层：请求、回复、命令、RESP
   write(fd, "+OK\r\n", 5)      ← 只是把字节抄进内核，【不发送】
        ↓ 拷贝
内核发送缓冲区 = 一条字节流
   +OK\r\n+OK\r\n+OK\r\n         ← 【消息边界在这里就没了】
        ↓ TCP 层【另一个时刻】独立决定切段
TCP 层：报文段、MSS、序号、ACK、窗口、Nagle
   它不知道什么叫"一个请求"，它只数【字节】
```

- `write` 三次 5 字节 和 `write` 一次 15 字节，**内核缓冲区里的字节完全一样**
- 一个报文段可以装 **半个** / **3 个** / **3.5 个** 请求 —— 没有任何对应关系
- **Nagle 站在"TCP 要切段发出"的那一刻**，不在 `write` 那一刻
- ⚠️ 混乱的典型症状：**用应用层的词（请求/回复）去问 TCP 层的问题（报文段/MSS）**
  > **自查口令：我现在说的这个名词，属于哪一层？**

**现象**：`redis-benchmark -c 50 -P 16` 时，cairn 吞吐从 55.7K **掉到 18K**（真 Redis 涨到 809K）。**"优化后反而变慢"是最值钱的一类性能问题——它说明有结构性缺陷，不是常数大。**

**定位过程（方法比结论重要）**：

| 步骤 | 观察 | 推出什么 |
|---|---|---|
| 看 CPU | cairn 4%、压测工具 4%，**两边都不干活** | 不是算力问题，**所有人都在【等】** |
| 写探针实测 | 一批 16 条命令往返 `min=42.04 p50=44.01 max=47.86 ms` | **延迟几乎是常数**（±3ms），与负载无关 |
| 判据 | "算得慢"的延迟会随负载变化；**常数延迟 = 有人在等一个定时器** | Linux 延迟 ACK 最小值正好 **40ms** |

**反推验算 —— "常数延迟"的铁证**（2026-09-16 补）：

```
每条连接：每 40ms 才完成一批 16 条命令
1 秒 = 1000/40 = 25 批  →  每连接 25 × 16 = 400 条/秒
50 条连接 × 400 = 20,000 条/秒 = 20K        实测 18K ✓ 对上了
```

> **QPS 不由你的代码速度决定，由那个 40ms 定时器决定。**
> 能用"延迟 × 并发"反推出实测 QPS，才算真的定位了；对不上说明机制猜错了。

**机制**（两个各自合理的优化撞在一起 = 病态交互 pathological interaction）：

- **Nagle**（发送侧）：还有**已发出但未被 ACK** 的数据在途时，不再发新的小包，先攒着 → 省带宽
- **延迟 ACK**（接收侧）：收到数据先别急着 ACK，**等最多 40ms** 看有没有数据可以**捎带**（piggyback）→ 省纯 ACK 包

```
cairn                                   客户端
write(回复1) → 【段1 立刻发出】────────► 收到段1
write(回复2) → Nagle: 段1 未被 ACK,      延迟ACK: 等等看有没有数据捎带
                且不满 MSS → 扣住         ⏳ 但客户端【没有东西要发】
write(3..16) → 全堆在内核发送缓冲区          （16 个请求早发完，正纯等回复）
                                        ⏳ 40ms 定时器到期 → 被迫单发裸 ACK
              ◄─────────────────────────
Nagle 放行 → 【回复 2~16 合并成一个大段一次发出】
```

**一批里只有第一个包溜出去，后面 15 个一起被扣。数据一个字节没丢，只是晚到 40ms。**

⚠️ **"一批只卡一次"≠"整个压测只卡一次"**（2026-09-16 在这句措辞上卡过）：**每一批都要卡 40ms，循环往复**——

```
40ms 到期 → 客户端发裸 ACK
  → 服务器 Nagle 放行，发出回复 2~16 的大段
  → 客户端收齐 16 个回复 →【这一批完成】→ 立刻发下一批 16 个请求
  → 这批请求【捎带了对大段的 ACK】→ 服务器未确认数据清零
  → write(回复1) → 第一个包，放行，飞出去
  → write(回复2) → 又被扣 →【又是 40ms】
```

**QPS 被钉死在 18K 正是因为它循环**——只卡一次不会有这个数（见上面的反推验算）。

**这个 bug 需要三件事同时成立**：① 服务端产生小包序列 ② 客户端此刻无数据可发 ③ Nagle + 延迟 ACK 都开着。**少一个都不发生——所以没有 pipelining 时它从不出现**（一问一答时客户端总有下一个请求可以捎带 ACK）。

**两处修复**：

| | 改动 | 效果 |
|---|---|---|
| ① `TCP_NODELAY` | `accept` 后一行 `setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, ...)`，直接关掉 Nagle | 18K → **205K（11.4×）** |
| ② **回复批量写** | 一轮事件循环里的回复全 append 进 `Connection::output`，**最后一次 `write` 发出** | 205K → **825K（再 4×）**，顺带把 16 次系统调用变 1 次 |

**②才是根治**：从源头不产生小包序列。真 Redis 两个都做了。

**为什么"批量写"能治 Nagle，而"Nagle 自己攒"不行 —— 字节数明明一样**（2026-09-16 的疑问，这是第 0 节分层的第二个用处）：

```
【Nagle 攒】write 16 次
  write(回复1)  5B  → 第一个包，Nagle 放行 ──► 飞出去了
  write(回复2)  5B  → 【想发】！但回复1 未被 ACK 且 5B < MSS → 扣住
  write(3..16)      → 继续扣            ⏳ 40ms

【批量写】  write 1 次
  outputAppend × 16 → 全攒在【应用层 output_】里，内核根本不知道有这些字节
  write(80B)        → 第一个包，Nagle 放行 ──► 飞出去了
                    → 【没有第二次 write】→ Nagle 没有可扣的对象
```

> **Nagle 扣的是"你想发的第二个包"，不是"缓冲区里的字节"。**
> ⚠️ 不是靠"凑够 MSS 突破"放行的 —— 一批回复通常几十到几百字节，**离 MSS(1460) 远得很**，靠的是"它是第一个包"。

| 字节攒在哪 | Nagle 看得见吗 | 后果 |
|---|---|---|
| **内核发送缓冲区**（Nagle 替你攒的） | ✅ 它管着，**要等 ACK 才放行** | 卡 40ms |
| **应用层 `output_`**（批量写攒的） | ❌ **压根不知道这些字节存在** | 无事发生 |

**字节数一样，但攒在哪一层，决定了 Nagle 管不管得着。批量写 = 把攒的地方从内核挪到应用层。**

**代码上就是 `send()` 的位置**：`main.cpp` 的 `while (res.status == OK)` 里只 `outputAppend`，**`send()` 在循环外面**。挪进循环就退化成 16 次 `write`，那个 4× 直接没了。

- **`write()` 就是 I/O**——I/O 的判据不是"碰没碰磁盘"，是**数据有没有跨出进程交给内核**。贵在**每次都要陷入内核**。
- **`write()` 返回只代表"内核收下了"，不代表"发出去了"**（note 条目 10 今天兑现）。**卡住的是内核 TCP 层的发送决策，不是你的代码**——这就是 CPU 只有 4% 的原因。
- **分层**：应用层的"回复" ≠ TCP 层的 ACK。redis-benchmark 的代码里只有 `read()`，它**看不见也不知道** ACK 的存在。"一个请求完成" = 收到完整回复字节，与 ACK 无关。
- **责任判据**：客户端的延迟 ACK 是标准行为，Nagle 也是。**一个只在你身上出现的问题，别去怪对面的标准行为。** 面试说"是客户端的锅"当场减分。
- **最深的一条**：**每个优化都带着一个关于"典型场景"的假设。假设不成立时，优化就变成纯损失。** 延迟 ACK 假设"接收方过会儿多半有数据要发"——在 pipelining 下这个假设直接崩了。这条判据适用于所有缓存、批处理、预取、延迟写。

### 23. Reactor 五个角色（设计已通，代码待写）🟡 2026-09-09

**Reactor 模式的定义很朴素：一个"等事件"的循环 + 一张"哪个事件交给谁处理"的对应关系。**
**cairn 现在跑的已经是一个 Reactor 了**，只是被压扁成一个 275 行的函数：`epoll_wait` 是那个循环，`if (ep[i].data.fd == server_fd) ... else ...` 是那张对应关系。**D6 不是"实现 Reactor"，是把已经存在的 Reactor 显形。**

| 角色 | 现在在 main.cpp 的哪 | 职责 |
|---|---|---|
| `Acceptor` | socket/setsockopt/bind/listen | 只管接新连接 |
| `Poller` / `EpollPoller` | epoll_create / epoll_ctl / epoll_wait | 只管"**怎么等**"事件 |
| `EventLoop` | `while(1)` + 那个 if/else | 只管"等到了**怎么派活**" |
| `Channel`（**还不存在**） | —— | fd + 关注的事件 + 读回调/写回调 |
| `Connection` | 裸 struct + 读写那一大段 | 一条连接的全部状态和行为 |

- **`Poller` 和 `EventLoop` 为什么要分开**：`Poller` 是"怎么等"（epoll/poll/select/kqueue **可换**），`EventLoop` 是"怎么派活"（**不变**）。
  > **判据：会变的和不会变的，切开。** （D11 接 RocksDB 时的 `IStore` 接口是同一招。）
- **`Channel` 是核心零件**：现在的分发是**硬编码**的 `if (fd == server_fd) ... else ...`，加一个定时器 fd / signalfd 就得再加一个 `else if`。Reactor 让**每个 fd 自己带着"我该怎么处理"**，分发变成一行 `for (auto* ch : active) ch->handleEvent();`。
  > **这就是"把 if-else 链换成数据自己携带行为"——和命令分发表是同一个思想**（`if (cmd=="SET")` → `map<string,{arity,handler}>`）。
- **`EPOLLOUT` 非阻塞写**（`Connection::output` 真正的用武之地）：
  ```
  write 返回 300（只收下 300，还剩 800）
    → output.retrieve(300)         剩下的留在 Buffer 里
    → EPOLL_CTL_MOD 加上 EPOLLOUT  "内核腾出空间了叫我"
    → 立刻返回，事件循环继续伺候别人   ← 关键：绝不阻塞
  EPOLLOUT 来了 → 接着写 → 写完 → EPOLL_CTL_MOD 【去掉】EPOLLOUT
  ```
  **最后那步必须做**：LT 模式下只要发送缓冲区可写就一直报 EPOLLOUT，忘了摘 = 100% CPU 空转（note 条目 15 同款）。
- **改监听事件用 `EPOLL_CTL_MOD` 一步到位**，不要 `DEL` 再 `ADD`——两次系统调用，中间还有个 fd 不在名单上的窗口期。

**实施顺序**（一个一个搬，每搬完跑 22 条验收）：`Connection` → `Channel` → `Poller`/`EpollPoller` → `EventLoop` → `Acceptor` → `EPOLLOUT`。搬到 `EventLoop` 那一步，`if-else` 会自己消失。

---

---

### 29. 边界：依赖向下，控制向上 🟡 2026-09-12（讲了第三遍才通，未复述）

**一句话**：`#include` 只能向下（编译期），调用上层只能靠回调（运行期）。

cairn 的依赖图（从 `#include` grep 出来的，不是画的）：
```
谁都不认识（叶子）   Buffer   Channel   Socket
        ↑
认识叶子            Poller→Channel   Connection→Buffer,Channel,Socket   Accept→Socket,Channel
        ↑
认识上面            EpollPoller→Poller   EventLoop→Poller
        ↑
认识所有人（根）     main   （唯一认识 MemStore / commands / RespParser 的地方）
```

**三个事实**：① `Channel` 零个项目内 include —— 所以它能当公共类型（一个谁都不依赖的东西，才能被所有人依赖）
② 箭头从不反向，没有环 ③ `src/net/` 没有任何文件认识 `store/` 或 `cmd/`

**一次事件：先一路向下，再通过回调一路向上**
```
向下（直接调，编译期知道对方是谁）   main→loop.loop()→poller_->poll()→epoll_wait()
向上（回调，编译期不知道对方是谁）   EventLoop→ch->handleEvent()→read_cb_()→on_message_(*this)→main 的 lambda
```

**为什么向上必须用回调**——这是硬约束不是风格：`Channel` 想直接调 `Connection`，就得 include 它，而 `Connection.h` 已经 include 了 `Channel.h` → 环 → 编译不过（见条目 28）。
**你什么时候需要回调？当你要调用的那个人在你上面的时候。**

**回调解决的不是"没资源"，是"不想知道"**：技术上完全可以 `Connection(int fd, uint32_t ev, MemStore &store)`，能工作。
不这么做是因为 `Connection.h` 就得 include `MemStore.h`，**这条连接从此只能说 RESP 协议**。

**边界是能 grep 出来的，不是感觉**：
```bash
grep -rn "store/\|cmd/\|proto/" src/net/      # 0 条 —— 网络层不认识业务
grep -n "epoll" Channel.h EventLoop.h Poller.h # 0 条 —— 只有 EpollPoller 认识 epoll
grep -rn "Connection" EventLoop.h Poller.h Accept.h  # 0 条
```

### 30. 决定"东西该放哪个类"的三个问题 🟡 2026-09-12

**① 换一个程序，这件事还一样吗？** 一样→进库，不一样→留 main
> 不用靠经验的替代判据：**这段代码里有没有出现只有本项目才有的名字？**
> `accept`/`TCP_NODELAY`/`epoll_wait` 是系统调用 → 进库；`MemStore`/`handleCommand`/`resp::parse` 只有 cairn 有 → 留 main

**② 做这件事需要认识谁？** 需要认识的人在你下面→直接调；在你上面→**这件事不该你做，改成回调交出去**

**③ 这个类被谁、多久驱动一次？** 这条决定接口长什么样
| 类 | 谁驱动 | 多久一次 | 于是接口要 |
|---|---|---|---|
| `EventLoop` | main | 一辈子一次 | 随便写，不用抠性能 |
| `Poller` | EventLoop | 每轮一次（每秒几万） | out 参数复用内存 |
| `Channel` | EventLoop | 有事件时 | —— |
| `Connection` | Channel（经 read_cb_） | fd 可读时 | —— |
| main 的 `on_msg` | Connection（经 on_message_） | 每条命令一次 | 签名带 `Connection&` |

**判断一个类参不参与某件事**：把回调内容整个换掉，这个类要不要改？不用改 = 它不参与。
（`EventLoop` 提供"每轮末尾这个时刻"，拿来干嘛它完全不在乎 → 它不参与"清理连接"。）

### 33. 触发者 ≠ 拥有者 🟡 2026-09-12

2026-09-12 在 `Connection.cpp` 的 `write_cb_` 上写错过一句注释：

> ~~"调用回调函数的是 channel，所以 send 内的 `output_` 也属于这个 channel"~~ ← **后半句错了**

真实的调用链：

```
Channel::handleEvent()
  └─ write_cb_()                ← 一个 std::function<void()>，Channel 只知道"有个活要干"
       └─ [this]{ send(); }     ← this 是 Connection*（lambda 写在 Connection 的构造函数里）
            └─ this->send()
                 └─ this->output_
```

`Channel` 手里只有一个 `std::function<void()>`。**它不知道里面捕获了谁、会碰哪块内存，也不该知道。**

| | 角色 | 知道什么 |
|---|---|---|
| `Channel` | **触发者** | "事件来了，喊一声" |
| `Connection` | **拥有者** | 喊的是什么活、动哪块内存 |

**可以 grep 验证**：

```bash
grep -n "Buffer" src/net/Channel.h     # 0 条
```

如果 `output_` 真属于 Channel，Channel 就得 `#include "Buffer.h"` —— 条目 29「依赖向下，控制向上」当场就塌了。

> **回调的全部作用，就是让"谁在编译期认识谁"和"谁在运行期驱动谁"方向相反。**
> `Connection.h` **include** `Channel.h`（依赖向下，编译期，箭头 Connection → Channel）
> `Channel` 通过 `std::function` **反向触发** `Connection`（控制向上，运行期，箭头 Channel → Connection）
> 两个箭头方向相反不是巧合，**这就是回调存在的理由**。

⚠️ 同一个误区的另外两种说法，都是错的：
- "`epoll` 拥有这些 fd" —— epoll 只是记下号码，fd 的生死归 `Socket`
- "`EventLoop` 拥有这些连接" —— `EventLoop` 只认识 `Channel*`，连 `Connection` 这个类名都没见过（`grep -n Connection src/net/EventLoop.h` → 0 条）

### 36. EPOLLHUP / EPOLLERR 的真实行为（实测，别信原理推演） 🟡 2026-09-14

**它们不走注册**：`epoll_ctl` 的 `events` 里写不写都无所谓，内核该报就报（`man epoll_ctl` 原话："epoll_wait will always report"）。
所以 `Channel` 的 `events_`（请求）里永远不会有它们，`revents_`（报告）里会。

**位运算**（`handleEvent` 的全部）：`|` 合并开关（注册时 `EPOLLIN | EPOLLOUT`）；`&` 检测某位（`revents_ & EPOLLIN`，结果只可能是 `EPOLLIN` 或 `0`）。
⚠️ `revents_ & EPOLLIN == 0` 是 `revents_ & (EPOLLIN == 0)` = 永远 0 —— `==` 优先级高于 `&`，CHECKLIST 第 4 条同族，**加括号**。

**2026-09-14 实测**（客户端有未读数据时 `kill -9` → RST）：
```
revents = 0x019   IN=1 ERR=1 HUP=1
read -> -1, errno=104 (ECONNRESET)
```
> **Linux 上已连接的 TCP socket，HUP 总是伴随 IN**（`tcp_poll`：进入 CLOSE 状态时置 `RCV_SHUTDOWN` → 亮 IN）。
> 所以只认 IN/OUT 的旧 `handleEvent` **在这个场景下不会 busy loop** —— read 返回 -1 自然走到 on_close_。

**「HUP 无 IN」真会出现的场景**：非阻塞 `connect()` 失败（socket 从没到 ESTABLISHED）→ `ERR|HUP`；`timerfd` / `eventfd` / 管道 → `HUP`。**Raft 阶段两个都会撞上。**
所以 `close_cb_` 是"不依赖 Linux 实现细节"的正确性防线（muduo 也这么做），**不是修今天的 bug**。

**处理顺序**：三个独立 `if` 时，`IN|ERR|HUP` 会让 `on_close_` 被调两次（close_cb_ 一次、read 返回 -1 一次）。现在没出事是因为 `cleanupClosed` 用 `find` 去重。
muduo 的写法从源头掐掉：`if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))` —— 有 IN 就交给读路径，顺便保证**正常 close 时残留数据先读完再关**。

**教训（Claude 的）**：2026-09-13 我从原理推"kill -9 → RST → HUP 无 IN → busy loop"，没实测就让他去修。实测推翻。**先复现再改，这条对出题的人同样成立。**

### 37. 不在回调栈里删自己 —— 延迟释放（pending_close = Redis freeClientAsync） 🟡 2026-09-14

**问题**：连接出错时，能不能在 `on_close_` 回调里直接 `con_map.erase(fd)` 把 Connection 删掉？

**不能。** 回调栈要原路退回去，每一层都站在被删对象上：
```
Channel::handleEvent()          ← Channel 是 Connection 的成员
  └─ read_cb_ → Connection::handleRead()
       └─ on_close_(*this) → ❌ 若此处 erase → Connection 析构 → channel_ 随之析构
       ← 回到 handleRead：this 已死
  ← 回到 handleEvent：this（Channel）已死 → use-after-free
```
`delete this` 是合法 C++，但只有"之后不再碰任何成员且没人在栈上"时才安全 —— 事件回调里永远不满足。

**解法 = 登记，等栈退完再删**（2026-09-11 已实现，当时不知道它有名字）：
| | 做法 |
|---|---|
| Redis | `freeClientAsync()` → `clients_to_close` 链表 → 每轮事件循环末尾 `freeClientsInAsyncFreeQueue()` |
| muduo | `queueInLoop([conn]{...})` → 本轮 loop 末尾的 pendingFunctors |
| cairn | `on_close_` 只 `pending_close.push_back(fd)` → `setAfterLoopCb(cleanupClosed)` 统一 erase |

**两道防线缺一不可**：登记制让"第二次调 on_close_"安全；`cleanupClosed` 里的 `find()==end() → continue` 让"pending_close 里的重复 fd"安全。
（2026-09-14 `IN|ERR|HUP` 同时到达时 on_close_ 被调两次，就是靠这两道活下来的。）

**两条原则**：
> **谁拥有谁删** —— con_map 拥有 Connection，所以 con_map 删；对象不自删。
> **不在回调栈里删自己** —— 登记，等栈退完。

面试可直接讲："连接关闭用延迟释放，和 Redis 的 freeClientAsync 同一思路。"
