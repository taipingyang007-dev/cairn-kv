# 排查速查（行为不对 + 慢）：看到什么 → 先做什么

> **规矩：不许盯着代码看。** 先按表选工具，让程序自己说话。
> **命令不用记，抄这里。** 这一页就是 debug 的"体系"，没有别的。

---

## 先分清是哪一类（走错方向，后面全白费）

| 症状 | 类别 | 去哪儿 |
|---|---|---|
| 崩溃 / 算错 / 卡死 / 结果不对 | **行为不对** | 往下看「五步流程」+「症状 → 第一个动作」 |
| **行为是对的，但慢** | **性能** | 跳到本文末尾的 **「性能排查（三步）」** |

> **两类问题的工具几乎不重叠**：行为不对用 ASan / gdb / printf；慢用 top / latency / tcpdump。
> **最贵的错误是拿错工具** —— CPU 明明空着，却去读代码优化算法，能优化到天荒地老。

---

## 五步流程（顺序不能换）

1. **复现** —— 找到最小、最稳定的复现方式。复现不了就先解决复现。
   **复现不出来 = 你要修的不是那个病。** 别改，先弄清病在哪。
   （2026-09-14：Claude 从原理推出 "HUP 无 IN → busy loop"，实测 RST 是 `IN|ERR|HUP`，旧代码根本没病。**出题的人也会犯这个错。**）
   （2026-09-15 又一次：Claude 推"持久化 → 数据残留 → 验收会坏"，实际数了 29 项断言，每项都自己先建状态，一项都不会坏。**推理链再顺，也要先去数一遍真实的东西。**）
2. **判断现象是「稳定的」还是「每次不同」** ← 这一步决定后面的分岔，别跳过
3. **选工具**（下表）
4. **定位** —— 工具会给你位置，你不用猜
5. **改完用同一个工具再跑一遍验证** —— **不是只看测试绿**
   （2026-09-12 教训：加个 `if` 绕开触发路径，29/29 照样绿，bug 一个字没动）

---

## 症状 → 第一个动作

| 看到什么 | 病灶类型 | 第一个动作 |
|---|---|---|
| 编译报一屏错 | — | **只读第一条**，后面多半是连带的。找 `note:` 行，它经常直接给答案 |
| `undefined reference to ...` | **链接期，不是编译期** | `CMakeLists.txt` 的 `add_executable` 漏了那个 `.cpp`（本项目已漏 4 次） |
| `does not name a type`，而那个类型明明存在 | 循环 include | 见 note 28 |
| 段错误 / 输出乱码 / **每次跑结果都不一样** | **内存** | **ASan**（见下方命令） |
| **稳定地**算错（每次错得一模一样） | **逻辑** | `fprintf(stderr, ...)` 打中间值；或 gdb 断点 + `print` |
| 程序卡住不动 | 阻塞 / 死锁 | `gdb -p <pid>` → `bt`；或 `strace -p <pid>` 看卡在哪个系统调用 |
| CPU 100% 但不干活 | busy loop | `gdb -p <pid>`，连按几次 `bt`，**反复出现的那个栈就是** |
| 内存越用越多 | 泄漏 | ASan 自带 leak 检测（进程退出时报） |
| **测试全绿但实际不对** | 测试没覆盖那条路 | **先别改代码**，先问"怎么才能走到那条路" |
| 偶发、重启就好 | 未初始化 / 竞态 | ASan + UBSan；多线程再加 `-fsanitize=thread` |
| **慢**（吞吐上不去 / 延迟高 / 吞吐卡在一个数不动） | **性能，不是 bug** | 🔻 **跳到本文末尾「性能排查（三步）」，从 `top` 开始** |
| 服务器行为怪但看不到任何输出 | 输出被缓冲吞了 | 调试输出一律 `fprintf(stderr,...)`。**stdout 重定向时是全缓冲，崩溃时全丢；验收脚本也吞 stdout** |

### 最关键的一条分岔

> **稳定复现 = 逻辑错 → gdb / printf**
> **每次不同 = 内存错或竞态 → ASan**
>
> **断点找不到内存错**，因为损坏点和崩溃点不在一起 —— 你在崩溃行看到的是受害者，不是凶手。

---

## 现成命令

### ASan / UBSan（内存问题首选）

```bash
rtk cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g"
rtk cmake --build build-asan -j

ASAN_OPTIONS=detect_stack_use_after_return=1 ./build-asan/bin/cairn 2>&1 | tee /tmp/asan.log
```

单个文件：

```bash
rtk g++ -Wall -Wextra -std=gnu++20 -g -fsanitize=address,undefined \
        -fno-omit-frame-pointer x.cpp -o /tmp/x
```

**报告只读四行**（其余包括 shadow bytes 全是噪音）：

| | 找什么 |
|---|---|
| ① | `ERROR: AddressSanitizer: ___` ← 什么毛病 |
| ② | 栈回溯里**从上往下第一个带你自己文件名的行** ← 案发现场 |
| ③ | `is located in ... in frame #0 ___` ← 这块内存属于谁 |
| ④ | `[a, b) '____' (line N) <== inside this variable` ← 被动的是哪个变量 |

### Debug 构建（让 assert 真的生效）

```bash
rtk cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug && rtk cmake --build build-debug -j
```

⚠️ 本项目**默认 Release（`-O3 -DNDEBUG`）→ assert 全部被删掉**。debug 一律跑 `./build-debug/bin/cairn`。

### gdb 最小集（逻辑问题 / 卡住）

```bash
rtk gdb -q ./build-debug/bin/cairn
(gdb) break Connection.cpp:66     # 打断点
(gdb) run
(gdb) print output_               # 看变量
(gdb) bt                          # 看调用栈
(gdb) frame 2                     # 切到第 2 层栈帧
(gdb) continue
```

卡住不动时，附到活进程上：

```bash
rtk gdb -p $(pgrep cairn)         # 然后直接 bt
rtk strace -p $(pgrep cairn)      # 看卡在哪个系统调用
```

### 本项目特有的坑

- **验收脚本会吞掉程序输出** → debug 第一步永远是**直接跑 `./build/bin/cairn`** 看 perror
- **WSL mirrored 模式连空端口会永久卡死**，不报 refused → 先确认服务器在跑
- `redis-cli` 7.0.15 **没有 `-t` 参数**，要超时用 `timeout N redis-cli ...`
- `Clock skew detected` → make 可能**跳过重编译**，你会对着旧二进制 debug。
  修：`rtk find src tests -type f -exec touch {} +`
  ⚠️ **`hwclock` 在 WSL 里不存在**，别去试（2026-09 踩过）


---

# 性能排查（三步，从粗到细）

> 2026-09-09 定位 Nagle × 延迟 ACK 走的就是这三步。
> **顺序不能换**：每一步砍掉一大类可能。反过来做（一上来就抓包）会淹死在细节里。

| 步 | 回答什么问题 | 工具 | 判据 |
|---|---|---|---|
| 1 | **有人在算吗？** | `top` | 两边 CPU 都低 = 没人在算，都在**等** |
| 2 | **延迟是常数吗？** | `redis-benchmark` 的 `latency summary` | 波动极小 + 绝对值异常 = **在等定时器** |
| 3 | **线上真实飞了什么？** | `tcpdump` | 报文段数量、包间时间差 |

## ⚠️ 铁律：压测原始输出必须存档

2026-09-09 那次定位的**原始输出没存**，一周后只剩四个结论数字（55.7K / 18K / 205K / 825K），
**参数、CPU 占用、延迟分布全丢了**，复盘只能靠回忆。

> **每次压测跑完，把完整输出整段贴进当天的文档**（`docs/D<N>-*.md`）。
> **数字不值钱，上下文才值钱。**

---

## 第 1 步：`top` —— 有人在算吗

```bash
rtk top          # 压测【正在跑的时候】另开终端看，跑完就没了
                 # q 退出
```

**只看 `%CPU` 一列，其他全是噪音。**

**唯一要记的规则：100% = 一个核。** 单线程进程上限 100%；4 线程进程上限 400%。

| 被测服务 | 压测工具 | 结论 |
|---|---|---|
| **90–94%**（单线程，已跑满） | 15–45%（4 线程，上限 400%） | ✅ **瓶颈在服务器，数字可信** ← 2026-09-15 D13 就是这个 |
| 77%（没满） | **96%**（满了） | ❌ **数字作废**：你测的是压测工具的上限 → 加 `--threads N`（note 21 ③，实测撞过两次） |
| **4%** | **4%** | 🔴 **没人在算，全都在等** ← 2026-09-09，整条定位链的起点 |

⚠️ **例外**：`top` 里单线程的 cairn 出现过 **110.9% / 114.0%**。超出 100% 的部分是 **RocksDB 的后台 compaction 线程**（D13 记录）。

---

## 第 2 步：`redis-benchmark` —— 延迟是常数吗

```bash
rtk redis-benchmark -p 6380 -t set,get -n 500000 -r 1000000 -d 100 -c 50 -P 16 --threads 4
```

### 参数表（漏了 `-r` / `--threads` 数字照出，但是假的）

| 参数 | 含义 | 不设会怎样 |
|---|---|---|
| `-p 6380` | 服务器端口 | 默认 6379，**打到真 Redis 上去了** |
| `-t set,get` | 只测这几个命令 | 默认测全套（LPUSH/SADD…），**cairn 只有 6 个命令，其余全报错** |
| `-n 500000` | 总请求数 | 默认 100000。**太小测的是计时器**：`-n 1000` 时 SET/GET 报出一模一样的 `34482.76`（note 21 ①） |
| `-c 50` | 并发连接数 | 默认 50 |
| `-P 16` | **pipelining：一次塞 16 条不等回复** | 默认 1（一问一答）。**Nagle×延迟ACK 的触发条件，少了它整个 bug 不出现** |
| `-d 100` | value 字节数 | 默认 3 字节，不真实 |
| `-r 1000000` | key 在 100 万个里随机选 | 🔴 **所有请求打同一个 key** → GET 全命中 memtable 最上层，「只掉 2 倍」是假象（2026-09-15 踩过） |
| `--threads 4` | 压测工具自己开 4 线程 | 🔴 **压测工具先跑满**，测的是它的上限不是服务器的（note 21 ③） |

### 输出怎么读

```
WARNING: Could not fetch server CONFIG      ← ① 不是错误
====== SET ======
  32 requests completed in 0.00 seconds      ← ② 回显参数，核对用
  1 parallel clients
  10 bytes payload

Summary:
  throughput summary: 32000.00 requests per second      ← ③ 每【秒】，不是每分钟
  latency summary (msec):                               ← ④ 定位就靠这一行
          avg       min       p50       p95       p99       max
        0.304     0.232     0.239     0.375     0.375     0.375
```

**① 那条 WARNING**：benchmark 开场发 `CONFIG GET` 想问配置，cairn 没实现，于是警告一句用默认值继续。**不影响结果**，但它是**另开一条连接**发的 —— 抓包时别和压测连接混在一起。

**② 回显参数**：每次先扫这块，确认「你以为设的」和「实际生效的」一致。参数打错（`-p` 写成 `-P`）在这里一眼看出。

**④ `latency summary` 六个数：**

| | 含义 | 怎么用 |
|---|---|---|
| `avg` | 平均 | **最没用**。999 个 1ms + 1 个 1000ms → 平均 2ms 看着没事，**但那个用户等了一秒** |
| `min` | 最快一次 | **理论下限**：这台机器最好能到多快 |
| `p50` | 一半请求比它快 | 典型体验 |
| `p95`/`p99` | 95% / 99% 比它快 | **线上真正看的**，长尾藏不住 |
| `max` | 最慢一次 | 最坏情况 |

### ★ 怎么看出「常数延迟」—— 两层判断，缺一不可

```
正常（2026-09-16）： min 0.232ms → max 0.375ms     max/min = 1.6 倍
出问题（2026-09-09）：min 42.04ms → max 47.86ms     max/min = 1.14 倍
```

**① 绝对值异常**：本机 loopback 往返是**微秒级**（正常 min = 232 微秒）。42ms = 42000 微秒，**慢了 180 倍**，本机通信不可能这么慢。

**② 波动极小 → 不是排队，是定时器**

```
排队慢     →  忙时排长队、闲时不排  →  波动【大】
等定时器   →  定时器不管你忙不忙    →  波动【极小】
```

出问题那组全挤在 42~48ms，**波动只有 ±7%**。

> **判据：常数延迟 = 有人在等定时器。**
> 剩下就是查表 —— **Linux 延迟 ACK 的最小值正好 40ms**（`TCP_DELACK_MIN`）。42 里多出的 2ms 才是真正的处理时间。

> **更根本的分岔（性能排查的第一问）：**
> **「慢」有两种 —— ① 干得慢：CPU 跑满，在算但算不过来 ② 等得久：CPU 空着，没人在算。**
> **`top` 一眼分开这两种。代码写得再烂也烂不出 4% 的 CPU。**

---

## 第 3 步：`tcpdump` —— 线上真实飞了什么

> **什么时候用**：怀疑 Nagle / 延迟 ACK；想知道「我 `write` 了 N 次，线上真的飞了 N 个包吗」；
> 想看 MSS 是多少；想确认某个延迟是不是在等定时器（40ms / 200ms 这种整数一眼可见）。
>
> **`strace` 站在【应用↔内核】的边界，`tcpdump` 站在【内核↔网线】的边界。两个数字不一样，就是分层的证据。**

#### 一、装（只需一次）

```bash
rtk sudo apt update && rtk sudo apt install -y tcpdump strace
```

#### 二、抓一次（整段粘贴；cairn 需另开终端跑着）

```bash
rtk sudo rm -f /tmp/nagle.pcap
rtk sudo tcpdump -i any -n -Z root 'tcp port 6380' -w /tmp/nagle.pcap &
sleep 2
rtk redis-benchmark -p 6380 -t set -n 32 -c 1 -P 16 -d 10
sleep 1
rtk sudo pkill tcpdump
sleep 1
echo "===== 抓到的包 ====="
rtk sudo tcpdump -r /tmp/nagle.pcap -n -ttt
```

| flag | 作用 | 不加会怎样 |
|---|---|---|
| `-i any` | 抓**所有**网卡 | `-i lo` 在 WSL mirrored 下可能一个包都抓不到 |
| `-n` | 不反查域名 | 慢，输出脏 |
| `-Z root` | **抓包后不降权** | tcpdump 默认降权到 `tcpdump` 用户，**再写 root 建的文件就 `Permission denied`** |
| `'tcp port 6380'` | BPF 过滤 | 抓到一屏无关流量 |
| `-w` / `-r` | 存文件 / 回放 | 刷屏，且打印本身干扰时序 |
| `-ttt`（回放时） | **显示距上一个包的时间差** | 看不出 40ms 这类定时器等待 |
| `&` + `sleep` | 抓包**后台跑**，压测同时进行 | **串行跑 = 抓完了才压测 = `0 packets captured`**（2026-09-16 踩过） |

#### 三、读输出：三个固定动作

1. **去重** —— `-i any` 在 loopback 上同一个包出现两次（`Out` 一次 `In` 一次）。**只看 `Out`**
2. **分连接** —— 按客户端端口号分组。redis-benchmark 开场会**另开一条连接**发 `CONFIG`（就是那条 `WARNING: Could not fetch server CONFIG`），别和压测连接混在一起
3. **只看 `length` ≠ 0 的行** —— `length 0` 是纯 ACK / 握手 / 挥手，不带数据

一行的读法：

```
00:00:00.000072 loopback0 Out IP 127.0.0.1.6380 > 127.0.0.1.46086: Flags [P.], seq 1:81, ack 849, win 532, length 80
└─距上个包多久                    └─谁发给谁（端口分方向）        └─P=PUSH .=ACK        └─【这段带几字节数据】
```

`[S]`=SYN握手　`[S.]`=SYN+ACK　`[P.]`=数据+ACK　`[F.]`=FIN挥手　`[.]`=纯ACK

**MSS 在第一个 `[S]` 包的 `options [mss 1460,...]` 里**，是握手时协商出来的。

#### 四、2026-09-16 实测存档（`-n 32 -c 1 -P 16`，TCP_NODELAY + 批量写都开着）

| 层 | 数量 |
|---|---|
| 应用层：SET 命令 | **32 条** |
| 应用层：`write()` 调用 | **2 次**（`send()` 在 while 外，每批一次） |
| TCP 层：服务器发出的带数据段 | **2 个**，每个 `length 80` |

- **80 ÷ 16 = 5 = 一条 `+OK\r\n` 的长度** → 16 个响应合并成 1 个报文段，**这就是「报文段 ≠ 请求」的铁证**
- **MSS = 1460，段只有 80 字节** → 差 18 倍。**Nagle 放行它跟 MSS 无关**，靠的是「它是这批的第一个包，此时没有未确认数据」
- **第二批的 ACK 被捎带在数据包里**（`[P.] seq 81:161, ack 1697`），比第一批少发一个纯 ACK 包 → **piggyback 的活体证据**
- **全程没有一个 40ms**，时间差都是几十微秒 → 两处修复都在生效
