# cairn 笔记 · C++ 语言与工程

> 编译链接 / 生命周期 / 传参 / 代码组织

> **用法**：每讲透一块，在这里落一条。复述时**盖住这个文件**，凭记忆讲，讲通了在条目上标 🟢 + 日期。
> **编号是全局唯一的身份，跨文件不重编** —— `PROGRESS.md` 里的"note 条目 N"永远指同一条。
> 索引在 [README.md](README.md)。

---

### 12. sizeof 陷阱族 + 盲写毕业考存档 🟢 2026-09-04

**规则一句话**：`sizeof` 给的**永远是字节数**。凡是 API 要"个数"或"内容长度"的地方，都不能直接把 `sizeof` 交进去。

| 写法 | 实际值 | 该写什么 |
|---|---|---|
| `sizeof(const char *p)` | 8（指针大小） | 用数组 `const char buf[]`，`sizeof` 才是内容大小 |
| `epoll_wait(fd, ep, sizeof(ep), -1)` | 元素数×12 字节 | `maxevents` 要**元素个数** → 1024。给大了内核会写出数组、踩烂栈 |
| `write(fd, buf+sent, sizeof(buf)-sent)` | 多发了结尾 NUL | `sizeof(buf) - 1 - sent`，且必须和循环条件 `sent < sizeof(buf)-1` 一致 |

**2026-09-04 盲写毕业考**（关笔记关编译器默写 epoll 主循环，`tests/epoll_test.cpp`，已删）：
- **结构满分**：socket→setsockopt→addr→bind→listen→epoll_create→ctl→while→epoll_wait→for→二分支→read 三分支→write 循环，一次成型。
- **真 bug 5 个**：① `sizeof(ep)` 当元素个数 ② write 长度少减 1 ③ **write 失败 close 后没 break（前一天刚修过，复发）** ④ accept 没判返回值 ⑤ `sockaddr_in addr;` 没 `{}` 初始化。
- **无关紧要（可查手册）**：头文件、`socket` 第三参数、`SOL_SOCKET`、`reinterpret_cast` 拼写。
- **两条自我认知修正**：① **我怕错了东西**——头文件和参数是可查的，危险的是"我以为写对了"的 `sizeof`；② **被指出来改掉 ≠ 学会了**——③ 号 bug 复发就是证据，盲写能测出这个差别。
- **抽象没长牢**：`write_all` / `close_client` 又被摊回内联，三处重复的 DEL+close 全回来了。**看到第二次重复就该有"该抽函数"的痒感。**

### 14. 生命周期与传参 —— 悬空指针那一族 🟡 2026-09-06

**核心规则**：**临时对象活到"整条语句的分号"为止。**

```cpp
const char *p = f().data();   // ← f() 造的临时 string 在分号处析构，p 立刻悬空
write_all(fd, f().data(), n); // ← 安全：同一条语句内，临时对象还活着
std::string s = f();          // ← 最省心：用变量接住对象本身，活到作用域结束
```

比喻：朋友租房（临时对象的内存），你抄下门牌号（`.data()`）。朋友退租后，门牌号还在，房子已经不是他的了。

- **实战踩坑（2026-09-06）**：`const char *b = handle_Command(order).data(); write_all(fd, b, sizeof(b)-1);` —— 两个 bug 叠一行：① 悬空 ② `sizeof(指针)`=8（条目 12 第三次出现）。恶心处：`+PONG\r\n` 恰好 7 字节，PING 看着是对的，ECHO 就错位。

**传参决策表**：

| 要拿这个参数干什么 | 怎么写 |
|---|---|
| **只读**，不留下 | `const T&` ← 90% |
| 只读且小类型（int / 指针 / string_view） | 按值 `T` |
| 要改调用方的东西 | `T&` |
| 要**吃掉它**（改完返回 / 存起来） | 按值 `T`，需要时 `std::move` |

- **sink parameter 惯用法**：`std::string ToUpperCopy(std::string s) { transform(...); return s; }` —— 按值传看似浪费，实则最优：传左值拷 1 次（本来就要改），**传右值直接移动、零拷贝**；若写成 `const string&` 再内部复制，右值也躲不掉拷贝。
- `toupper` 必须先转 `unsigned char`（负 char 是 UB）。
- **返回值不用纠结**：直接返回对象，C++17 起 RVO 保证不拷贝。**永远不要为"避免拷贝"而返回引用/指针**。
- **引用 vs 指针**：引用 = 一定有且不换人；指针 = **可能没有**（nullptr）或要重新绑定。`Connection* conn = it->second.get();` 用指针对——查找可能失败。
- **`.get()`**：返回裸指针但**不转移所有权**，unique_ptr 仍负责析构。裸指针有效期 = map 里那个条目的存活期，`erase` 后立刻悬空。
- **"零拷贝"祛魅**：工业上指 ① `sendfile`/`splice`（内核态不过用户态）② `string_view`/`span` 视图 ③ 移动语义。不是"一次拷贝都没有"。**过早优化拷贝是新手常见错误——先跑通，压测看 perf 热点，再优化。** 面试答"我测了，瓶颈不在那"比"我优化了 X"值钱。

### 17. 编译与链接 —— 四个阶段，两个字母 🟢 2026-09-07

**一条 `g++ a.cpp b.cpp -o x` 背后跑了四个独立程序**，各管各的：

| 阶段 | 程序 | 输入 → 产物 | 干了什么 | 报什么错 |
|---|---|---|---|---|
| 预处理 | cpp | `.cpp` → `.i` | **只做三件事**：展开 `#include`（把整个头文件的内容替换进来）、展开宏、处理 `#ifdef`/`#pragma once`。**产物还是 C++ 源码，一行汇编都没有** | 找不到头文件 |
| 编译 | cc1plus | `.i` → `.s` | 语法/类型检查、优化，生成**汇编文本**。函数名在这里被 **name mangling** 编成 `_ZN4resp12simpleString...`（把参数类型编进名字，好支持重载） | `error: expected ';'` 这类 |
| 汇编 | as | `.s` → `.o` | 汇编翻成机器码。产物是二进制（开头 `\x7fELF`），**随身带一张符号表** | 基本不出错 |
| 链接 | ld | 一堆 `.o` → 可执行文件 | **把所有 `.o` 的符号表叠起来配对**。它不懂 C++、不懂类型 | `undefined reference` / `multiple definition` |

**编译单元（TU）= 一个 `.cpp` + 它递归展开的所有 `#include`。** 不是"那个文件"，是展开后那坨大文本。实测：`main.cpp` 317 行 → `main.i` **77494 行**；只写 `#include <string>` 的 2 行文件 → 31828 行。

**编译器一次只看一个 TU，看完就忘。** 所以编译 `main.cpp` 时它根本不知道世上还有 `RespParser.cpp`。

#### 符号表只有两个字母

```
T  = 我【提供】这个符号（函数）。全局变量是 D（有初值）或 B（无初值）
U  = 我【需要】这个符号，谁给我？
```

**链接器的全部工作**：让每个 `U` 配到 `T`。规则是 **N 个 U : 1 个 T**——一个 `T` 可以被任意多个 `U` 共用，但提供方全场只能有一个。

| 错误 | 含义 | 典型原因 |
|---|---|---|
| `undefined reference` | `U` 配到 **0 个** `T` | CMake 的 `add_executable` 漏列了那个 `.cpp` |
| `multiple definition` | 一个符号有 **2 个以上** `T` | **函数体（或变量定义）写在了 `.h` 里**，被多个 TU 各展开一份 |

#### 判据（自己的话，2026-09-07 复述通过）

> **`T` 的个数 = 这个函数体被几个 TU 看见过。**
> 函数体在 `.cpp` 里 → 只有那一个 TU 看得见 → 永远 1 个 T
> 函数体在 `.h` 里 → 有几个 `.cpp` include 它，就有几个 T

- **`#pragma once` 只在【一个 TU 内】生效**，防的是同一个 TU 里重复展开。**跨 TU 完全无效**——两次独立的预处理互不知情。这是最容易误解的一点。
- **变量和函数一样**：`.h` 里写 `std::unordered_map<...> kTable;` 同样炸 multiple definition（实测符号是 `B`）。
- **修法两条**：**路 A** 加 `inline`（豁免条：允许多份定义，链接器挑一份留下）；**路 B** `.h` 只留声明、函数体搬 `.cpp`（T 天然只剩 1 个）。**默认选 B**——A 的代价是"改一行实现，所有 include 它的 TU 全部重编"。
- **类内定义的成员函数隐式 `inline`**——STL 头文件全靠这条。（`Buffer.h` 走的是路 B，不是这条；2026-09-07 我一度说错。）
- **`constexpr` / 命名空间作用域的 `const` 变量默认内部链接**，所以 `constexpr const char* kNullBulk = "$-1\r\n";` 放 `.h` 里不炸。
- 乱码符号 `| c++filt` 一管就变人话。
- **报错归属判据**：**带文件名+行号 → 编译器，问题在一个 TU 内；带 `.o` 和符号名 → 链接器，问题在"谁提供/谁需要"。** IDE 的红波浪线是第三个程序（IntelliSense）在猜，不算数。

### 18. 函数指针 vs std::function 🟡 2026-09-07（讲了，未复述——欠一次）

| | `int (*)(int,int)` | `std::function<int(int,int)>` |
|---|---|---|
| 大小 | **8 字节**（就是一个地址） | **32 字节**（内含小缓冲区） |
| 能装 | 自由函数、**无捕获** lambda | 任何可以被 `()` 调用的东西（类型擦除） |
| 装不下什么 | **有捕获的 lambda** | —— |
| 代价 | 零 | 可能堆分配 + 一次间接调用，编译器难内联 |

**为什么有捕获的 lambda 塞不进函数指针？** 因为 lambda 是一个**对象**——编译器悄悄生成了一个类，**捕获的变量就是它的成员变量**。实测 `sizeof`：无捕获 = 1 字节（空类最小尺寸），捕获一个 `int` = 4 字节，捕获 64 字节的对象 = 64 字节。**函数指针只有 8 字节，只够存"代码在哪"，没地方揣数据。**

#### 参数 ≠ 捕获（这是关键分野）

```
创建时 ──────────────► 调用 ─────► 调用 ─────► 调用
  ↑                     ↑          ↑          ↑
捕获在这里给            参数在这里给（每次都能不一样）
给一次，之后一直揣着     调用方给，用完就扔
```

- **参数**属于"这一次调用"，函数不存它 → `add(1,2)` 和 `add(9,9)` 是**同一个函数、同一个地址**
- **捕获**属于"这个可调用对象"→ `[base=100]` 和 `[base=200]` 代码一样，却是**两个不同的对象**

> **判据不是"有没有数据"，是"这个数据是调用方给的，还是它自己揣着的"。**
> 调用方给（当参数传）→ **函数指针**
> 自己揣着（创建时装进去，调用方不知情）→ **std::function**

cairn 的命令 handler：`args` 和 `MemStore&` 都由调用方传 → 纯代码 → **函数指针**。
线程池的任务：`[=]{...}` 带捕获 → 非 `std::function` 不可。**同一个问题，答案取决于要不要捕获。**


**2026-09-14 复述通过（🟢）。** 他的原话：
> "lambda 的本质是创建一个 struct，里面存了 this，然后 operator() 重载了内部执行这个函数。"
> "memStore 存在于 main 的栈帧，而 resp::parse 是全局的。"

精度修正：存的是 **this 指针（8 字节地址）**，不是对象。Connection 先死 → 地址悬空，与 `[&client_conn]` 同族。

**lambda 脱糖**（编译器实际生成的东西）：
```cpp
[this]() { handleRead(); }
// ≈
struct __Lambda { Connection *this_;  void operator()() const { this_->handleRead(); } };
```
**捕获列表 = 成员变量；函数体 = operator()。** `[]` 空捕获就是 struct 里没有 `this_` → `error: 'this' was not captured for this lambda function`。

**捕什么、不捕什么的判据**：
> **捕局部变量（住在某个栈帧里的东西）。全局符号（自由函数 / 命名空间函数 / 全局变量 / static 成员 / 宏 / 类型）不捕，它们的地址链接时就钉死了。**

**追问：`on_msg` 写在 main.cpp、在 Connection.cpp 里被调，Connection.cpp 没 include RespParser.h，为什么能用 `resp::parse`？**
> **编译看的是「写在哪」，不是「从哪调」。** 名字查找和类型检查只在 main.cpp 编译时发生一次，之后 lambda 的 operator() 就是机器码，里面是 `call <地址>`。Connection.cpp 调 `on_message_(*this)` 只是跳进那段编好的代码，不需要认识任何名字。**运行时没有 `#include` 这个概念。**
> 这就是 note 29「依赖向下，控制向上」在机器层面的样子：业务逻辑在 main.cpp 编好，装进 std::function 交给 Connection 拿着 —— Connection 手里是个黑盒。

### 19. 代码怎么分家：命名空间 / 头文件 / 类 🟡 2026-09-07（讲了，未复述——欠一次）

#### 命名空间按【概念】划分，不按文件

- **命名空间是开放的**：多个头文件可以往同一个 namespace 里追加，编译器把它们合并。`std` 就是活证据——`<string>`、`<vector>`、`<iostream>` 几百个头全往 `std` 里加，从不冲突。
- **"冲突"的判定是"完全限定名一样且指不同东西"**。`resp::parse` 和 `resp::error` 名字不同，天然不撞。
- 命名空间真正防的是**跨库撞名**（muduo 的 `Buffer` vs 你的 `Buffer`）。
- **一个 `.h` 一个 namespace 是反模式**——它把物理组织泄漏进 API。调用方凭什么要知道你把代码分了几个文件？
- > **文件是给写代码的人分的，命名空间是给用代码的人分的。**

#### 头文件是一份承诺书

> **写进 `.h` 的每一样东西，都是在说"我保证这个一直在、一直长这样"。只在自己家里用的，别写进承诺书。**

`kCommands` 那张表、各 handler 的实现——调用方从不需要知道它们存在 → 全进 `.cpp`。放 `.h` 里的三个具体代价：① 为了初始化表，所有 handler 声明都得搬进 `.h` ② 表不是 `const`，谁都能 `.clear()` 它 ③ 改一条命令，全项目重编。

**`inline` 能让它编译过，但"能编译"≠"该这么写"。**

#### `private` 挡不住 `.cpp` 能挡的

```cpp
class X { private: std::unordered_map<...> table_; };  // 这一行照样写在 .h 里
```
`private` = "**你不能碰**"；`.cpp` = "**你根本不知道它存在**"。**后者更彻底**——编译依赖也一并断掉了。所以"收窄接口"这件事，`.h/.cpp` 分离做得比类更好。

#### 什么时候该用类

> **判据：类是用来把【状态】和【操作这个状态的方法】绑在一起的。没有状态，就不需要类。**

| | 有状态？ | 该是类？ |
|---|---|---|
| `MemStore` | 有（那张 map 一直在变） | ✅ 且将来要换 RocksDB，靠接口切换 |
| `Buffer` / `Connection` | 有（每个连接一份） | ✅ |
| 命令表 | **没有**（启动时建好，之后只读，全进程一份） | ❌ |
| `resp::parse` | 没有（纯函数） | ❌ namespace 里的自由函数就够 |

没状态还包成类 = **披着对象外衣的全局变量（单例）**，调用方还得多持有一个不携带任何信息的对象。多写代码，零收益。

---

### 24. 特殊成员函数的生成规则 🟡 2026-09-11（讲了，未复述）

六个：默认构造 / 析构 / 拷贝构造 / 拷贝赋值 / 移动构造 / 移动赋值。

| 你手写了… | 编译器还生成什么 |
|---|---|
| 什么都没写 | 六个全生成 |
| **析构函数** | 移动构造/移动赋值 **不生成**；**拷贝照常生成** |
| 拷贝构造 或 拷贝赋值 | 移动 **不生成** |
| **移动构造 或 移动赋值**（哪怕 `= delete`） | 拷贝被**隐式 delete** |
| 任意构造函数 | **默认构造不生成** |

**为什么"手写析构 → 不生成移动"**：你手写了析构，说明这个类管着资源；那东西怎么"搬"编译器不知道，不敢猜。
**那拷贝为什么还生成**：C++98 遗产，C++11 不敢破坏旧代码（标准里标了 deprecated，一直没删）。

这条在 cairn 里咬过三次：`EpollPoller` / `EventLoop` / `Socket` 都是只写了析构，拷贝还在 → 两个对象持有同一个 fd。
`Channel` 反而是安全的——那行 `Channel(Channel&&) = delete` 顺手把拷贝也封了。

> 判据：**类里有裸资源 → Rule of Five → 五个都要表态。**

### 25. 成员初始化的两个坑 🟢 2026-09-12（当天踩了第二个，自己用 perror 定位）

**坑一：成员按【声明顺序】构造，与初始化列表的书写顺序无关。**
所以声明在前的成员，可以被后面的成员拿来用：

```cpp
Socket  socket_;    // ① 先构造
Channel channel_;   // ② 后构造 → channel_(socket_.getFd(), ...) 安全
```
`-Wreorder` 防的就是"书写顺序和实际顺序不一致"，而**实际顺序决定了谁能用谁**。

**坑二（更隐蔽）：成员的构造【参数】，在这个成员自己构造之前求值。**

```cpp
Accept::Accept(uint16_t port, int backlog)
    : socket_( createListenFd(port, backlog) )   // ← 这个函数跑完，socket_ 才开始构造
```
`createListenFd` 里访问 `socket_.getFd()` → 读一块还没初始化的内存 → 运行时 `Bad file descriptor`。

**这两个坑要分开**：声明顺序管"成员之间"；坑二是"同一个成员，参数先于自己求值"。**`Socket` 排第一个也救不了坑二。**

> 根治：把这种辅助函数写成 **`static`**（或匿名 namespace 的自由函数）——加了 `static` 编译器会直接拒绝访问成员，同样的 bug 再也写不出来。
> **能让编译器挡住的错误，就别靠记性挡。**

### 26. most vexing parse 🟡 2026-09-11（一天踩两次）

```cpp
EventLoop loop();          // 不是建对象，是【声明了一个返回 EventLoop 的函数】
void closeEpollFd();       // 在函数体里，这是【函数声明】，不是调用
```

标准规定：**语法上能读成声明的，一律读成声明。** 编译器不会警告，因为完全合法。

- 错误信息里的 `EventLoop()` 带一对括号 —— 那是"函数类型"，不是 `EventLoop`
- **根治：用 `{}` 初始化。`EventLoop loop{};` 永远不会被读成函数声明。**

> debug 技巧：编译器报类型错时，**把它说的类型一个字符一个字符读一遍**。你的眼睛会自动忽略括号，编译器不会。

### 27. 继承 vs 组合 🟡 2026-09-12（讲了，未复述）

```cpp
class EpollPoller : public Poller       // 继承 is-a
class EventLoop { unique_ptr<Poller> poller_; }   // 组合 has-a
```

**判据：把"A 是一个 B"念出来，通不通顺？**
- "EpollPoller 是一个 Poller" ✅ —— 它能被当成 Poller 传给任何要 Poller 的地方
- "EventLoop 是一个 Poller" ❌ —— EventLoop 没有 `poll()`，传不过去

⚠️ **`#include` 只说明"我认识它"，不说明"我是它"。** 认识的方式有两种：继承和持有。（2026-09-12 这里混过一次。）

### 28. 循环 include 的报错形态 🟢 2026-09-12（亲手撞了一次）

`Channel.h` 里 `#include "Connection.h"` + 一个 `Connection *owner_`，报的是：
```
Channel.h:16: error: 'Connection' does not name a type
```

展开顺序：
```
main.cpp → Connection.h ①开始处理（pragma once 打上标记）
              ↓
           Channel.h ②开始处理
              ↓
           Connection.h ③ 已在处理中 → 跳过，返回空
              ↓
           Channel.h ④ 继续 → 用到 Connection → 定义还没走到 → 💥
```

**`#pragma once` 阻止了无限递归，但没阻止"用到还没定义的东西"** —— 它把死循环变成了一个看起来莫名其妙的错误。

⚠️ **谁先被展开，报错的就是另一个** → 循环依赖的报错位置会随编译顺序漂移，这是它难查的主因。
⚠️ **前置声明（`class Connection;`）能让它编译过，但设计上的环还在** —— `Channel` 仍然认识 `Connection`，于是监听 socket 没有 `Connection`，`owner_` 填什么？**编译器放过你了，类却不能复用了。**

> 看到 `does not name a type` 而那个类型明明存在 → 先查循环 include。

### 31. `assert` 在 Release 下根本不存在 🟡 2026-09-12

`<cassert>` 里 `NDEBUG` 那一支把 `assert(x)` 定义成 `((void)0)` —— **整行在最终二进制里不存在**。

而 cairn 的 [CMakeLists.txt](../../CMakeLists.txt) **默认就是 Release**，CMake 的 Release 自带 `-DNDEBUG`：

```bash
$ grep CXX_FLAGS build/CMakeFiles/cairn.dir/flags.make
CXX_FLAGS = -O3 -DNDEBUG -std=gnu++20 -Wall -Wextra
```

实测（2026-09-12，用项目的真实 flags）：

```
$ g++ -O3 -DNDEBUG -D_FORTIFY_SOURCE=2 ... t1.cpp && ./t1
assert 没拦住我
exit=0
```

> **到今天为止，你写的每一个 assert 一次都没跑过。**

⚠️ 这不是说 assert 没用。它的定位是 **开发期吵闹、生产期消失**（note 15）。问题是你**从来没在 Debug 下编译过**，所以连"吵闹"那一半也没拿到。要拿到，得另开一个构建目录：

```bash
rtk cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug && rtk cmake --build build-debug
```

⚠️ **assert 不能当兜底**。分工是死的：
- **不变量**（`n <= readable()`，调用方的 bug）→ assert
- **能被外部输入触发的错误**（对端断开、write 失败、协议格式错）→ **必须 `if`**，因为生产环境里 assert 不存在

⚠️ 2026-09-12 复核：[net.md](net.md) 条目 13 里那句"验收 5 实测：故意改错在 -O3 -DNDEBUG 下照样 abort"与本条冲突。上面 t1 证明裸 assert 在 `-DNDEBUG` 下完全不触发，那次 abort 大概率是 `_FORTIFY_SOURCE=2` 抓的越界写，不是 assert。

### 32. 无符号减法下溢 —— `0 - 6` 不是 `-6` 🟡 2026-09-12

```cpp
size_t Buffer::readable() const { return writer_index_ - reader_index_; }
```

`size_t` 是**无符号**的。`reader_index_` 一旦越过 `writer_index_`，这个减法不会得到负数，而是**回绕**。

实测（2026-09-12，在一个只有 5 字节的 Buffer 上 `retrieve(99)`）：

```
retrieve 前  readable = 5
retrieve(99) 后 readable = 18446744073709551522      ← 2^64 - 94
exit=0                                               ← 没有任何报错
```

下一次 `readableView()` 会返回一个 **长度 1800 京字节** 的 `string_view`，喂给 `write()` → 段错误。
**而栈回溯指向 `write`，离真正的犯罪现场（那次 `retrieve`）差着十万八千里。** 这类 bug 难查的根子就在这：**损坏点和崩溃点不在一起。**

> C++ 里长度几乎全是无符号：`size_t` / `.size()` / `strlen` / `sizeof`。
> **写 `a - b` 之前先问一句：b 有没有可能比 a 大？**

三个常见现场：

```cpp
for (size_t i = 0; i < v.size() - 1; ++i)   // v 为空时 → 循环 1800 京次
if (v.size() - n > 0)                       // 永远为真，这个判断等于没写
idx_a - idx_b                               // 两个下标相减，顺序反了就是天文数字
```

⚠️ **`-Wall -Wextra` 抓不到它** —— 无符号回绕是**标准明确定义的合法行为**，不是 UB，编译器没立场报警。GCC 的 sanitizer 也默认不管。
**能挡住它的只有两样：写之前念那句判据，和一个真的会跑的 assert（见条目 31）。**

### 34. 定一个函数签名的三问法 🟡 2026-09-12

note 30 的三问法是**模块**粒度（东西该放哪个类）；这一条是**函数**粒度（一个函数的参数和返回值怎么定）。

> **① 谁来调它？**　→ 调用者手里有什么，决定参数**能**传什么
> **② 它需要认识谁？**　→ 要认识的人在你**下面** → 直接调；在你**上面** → **当参数传进来**（这就是回调存在的理由，见条目 29/33）
> **③ 调用者拿到结果要干什么？**　→ 决定返回值；没人用就是 `void`

2026-09-12 现场（`modEpoll_EPOLLIN_` 该是什么签名）：

```
① 谁来调？        Connection::send()，它手里有 *this
② 需要认识谁？    EventLoop —— 在 Connection 【上面】→ 不能 include，只能外面传进来
                  那个 Connection —— 调用者手里就有 → 当【参数】传，不要捕获
③ 结果干什么？    什么都不用 → void

⇒  std::function<void(Connection &)>
```

⚠️ **写错版本的教训**：一开始签名是 `std::function<void()>`，于是 lambda 只能 `[&client_conn]` **按引用捕获一个局部 unique_ptr** → 栈帧返回后悬空 → `stack-use-after-return`（ASan 实测，见条目 35）。

> **签名少一个参数，就得靠捕获补上；捕获补上的东西，生命周期就得你自己管。**
> 反过来说：**能当参数传的，绝不要靠捕获。** 参数在调用那一刻才绑定，永远指向活着的对象。

**怎么练**：接口设计靠积累"见过的签名形状"，不靠记规则。已有的模板可以倒推 ——
`on_message_` / `on_close_` 都是 `void(Connection &)`，照着它们反推就得到了 `modEpoll_EPOLLIN_` 的形状。

### 35. 栈帧 · stack-use-after-return · 怎么读 ASan 报告 🟡 2026-09-12

**栈帧**：每调用一个函数，就在栈上划一块地放它的局部变量；**函数一返回，这块地立刻还回去**。

```
 低地址 │ [&](int client_fd) 的栈帧 │ ← main.cpp:75  client_fd, client_conn
        │ Acceptor::handleRead      │
        │ Channel::handleEvent      │
        │ EventLoop::loop           │
 高地址 │ main 的栈帧               │ ← acpt, loop, con_map, memStore（活到程序结束）
```

⚠️ **栈帧弹掉 ≠ 内存被清零**。那几个字节还在、值也还在，只是**不再属于你**——下一次函数调用会把这块地拿去用。
所以这类 bug **"看心情"发作**：有时读到旧值（看起来完全正常）、有时读到别人的数据、有时段错误。**比空指针难查十倍**，因为空指针每次都崩。

**判据**：
> **按引用捕获 `[&x]` 之前问一句：`x` 住在哪个栈帧里？那个栈帧活多久？**
> `[&loop]` 安全（loop 在 main 的栈帧，活到程序结束）；`[&client_conn]` 致命（在那个 lambda 的栈帧，一返回就没）。
> **同一个 `[&]`，捕两个东西，一个安全一个致命 —— 差别不在写法，在那个变量住哪儿。**

## 工具：AddressSanitizer

```bash
rtk cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g"
rtk cmake --build build-asan -j
ASAN_OPTIONS=detect_stack_use_after_return=1 ./build-asan/bin/cairn 2>&1 | tee /tmp/asan.log
```

| flag | 作用 |
|---|---|
| `-fsanitize=address` | 每次读写内存前插一段检查代码，**在越界/悬空的那一刻就报**，不等它自己发作 |
| `-fsanitize=undefined` | 顺带查未定义行为（整数溢出、空指针解引用…） |
| `-fno-omit-frame-pointer` | 保住栈回溯的完整性，不然栈帧会缺 |
| `ASAN_OPTIONS=detect_stack_use_after_return=1` | **栈帧返回后的读写默认不查**，要显式打开 |

## 报告四十行，只有四行有用

```
① ERROR: AddressSanitizer: stack-use-after-return       ← 什么毛病
②   #3 in operator()  src/main.cpp:82                   ← 谁在读（从上往下第一个【你自己的文件】）
③ Address ... is located in stack of frame
      #0 in operator()  src/main.cpp:75                  ← 这块内存属于哪个栈帧
④   [64, 72) 'client_conn' (line 76) <== inside this variable   ← 被读的是哪个变量
```

其余（`invoke.h`、`std_function.h`、shadow bytes 那一大片）**全是噪音，专业人士也不看**。

> 段错误不要猜、不要盯着代码看 —— **用 ASan 跑一遍，它会在犯罪现场报警，而不是等尸体浮上来。**

## 调试工具是分层的，别一上来就 gdb

| 层 | 工具 | 什么时候用 |
|---|---|---|
| ① **让程序自己报警** | **ASan / UBSan**、`assert`、日志 | **内存问题首选** —— 在损坏发生的那一刻报，不等它发作 |
| ② 事后验尸 | core dump + `gdb`，`bt` | 已经崩了，看崩时栈长什么样 |
| ③ 交互式断点 | `gdb` 断点 / 单步 / `print` | **逻辑问题**：值算错了，想看中间值 |

> **断点适合「逻辑错」，不适合「内存错」。**
> 内存错的**损坏点和崩溃点不在一起** —— 在崩溃行打断点，看到的是**受害者不是凶手**。

2026-09-14 练习 1 现场：崩在 `std::string::_M_data()`（标准库内部），真正的犯罪现场是 `registerJobs` 早已返回的栈帧。**断点找不到，ASan 一跑就出。**

## 内存 bug 的签名特征

> **同一个二进制、同样的输入，每次跑结果都不一样 → 内存问题第一嫌疑。**

练习 1 的普通版连跑两次，乱码内容不同（`[??\n ?]` vs `[p????]`）。因为那块栈地已归还，后来谁用过、留下什么垃圾每次都不同。
表现有三档：**乱码 / 完全正常（旧值还没被覆盖，最可怕）/ 段错误**。

对照：**逻辑 bug 是稳定复现的。** 不稳定 = 内存或并发，不是逻辑。

## gdb 最小集（等真遇到"值不对/卡住"再用）

```bash
rtk gdb -q ./build-debug/bin/cairn
(gdb) break Connection.cpp:66   # 打断点
(gdb) run                        # 跑
(gdb) print output_              # 看变量
(gdb) bt                         # 看调用栈
(gdb) frame 2                    # 切到第 2 层栈帧
(gdb) continue                   # 继续
```
卡住不动时：`rtk gdb -p <pid>` 附到活进程上，直接 `bt` 看卡在哪。

### 38. char / char* / char[] / string —— 字节、地址、长度 🟡 2026-09-15

**底层只有两样东西：字节 和 地址。**

| 类型 | 是什么 |
|---|---|
| `char` | 一个字节（`'A'` 就是 65） |
| `char *` | 一个**地址**，8 字节。`int*` / `Connection*` 也都是地址，区别只在"从这个地址起按什么规格解读" |
| `char[N]` | N 个字节连在一起，**编译器知道有多大** |
| `char *[N]` | N 个地址连在一起（`argv` 就是这个） |
| `std::string` | 一个**类**：指针 + 长度 + 容量，管理堆内存 |

**C 没有字符串类型**，所谓字符串 = 一串 char + **末尾一个 `\0` 哨兵**：
```
char s[] = "abc";     'a' 'b' 'c' '\0'      → sizeof(s) == 4，不是 3
```
因为 `char*` **不带长度**，`printf("%s")` 只能靠找 `\0` 决定打到哪儿停。三个后果：
- `strlen` 是 **O(n)**；`string::size()` 是 O(1)
- 少了 `\0` = 一直往后读别人的内存（note 12 的 write 少减 1 同族）
- **中间不能有 0 字节 → C 字符串存不了二进制**。这就是 `std::string` 存在的理由

**数组退化（array-to-pointer decay）——「sizeof 陷阱」的根**：
```c
void f(char s[])   // 骗人写法，编译器读成 ↓
void f(char *s)    // 真签名，sizeof(s) == 8
```
**数组一传进函数就变成首地址，长度当场丢失。** note 12 的 `sizeof(const char *p) == 8` 不是 sizeof 有 bug。

**argv**：`int main(int argc, char *argv[])` 即 `char **argv`
```
./cairn R  →  argv[0]→"./cairn\0"   argv[1]→"R\0"   argv[2]→nullptr     argc==2
```

⚠️ **2026-09-15 撞的坑**：
```cpp
if (argv[1] == "M")               // ❌ 比两个【地址】，永远为假，编译器不报错
if (std::string(argv[1]) == "M")  // ✅ string 的 == 比内容
if (strcmp(argv[1], "M") == 0)    // ✅ 逐字节比到 \0
```
字符串字面量 `"M"` 也退化成地址。**CHECKLIST 第 4 条同族：写的 ≠ 想的，编译器沉默。**

**转换**：`string(p)` 读到 `\0` 为止并复制 / `string(p, n)` 不看 `\0`，能装二进制 / `s.c_str()` 给 C API（保证末尾 `\0`）。
⚠️ `.c_str()` 的指针归 string 所有，string 一改一析构就悬空（note 14 / 35 同族）。

**项目里三个同款，同一个设计**：
```
std::string_view       指针 + 长度，不拥有   ← Buffer::readableView()
rocksdb::Slice         指针 + 长度，不拥有   ← Put/Get 的参数
const char* + size_t   指针 + 长度，不拥有   ← Buffer::append(data, len)
```
> 都在解决同一件事：**`char*` 没长度，`\0` 又存不了二进制 → 把地址和长度捆着传，且不复制。**

**判据**：
> **要"拥有并管理" → `std::string`；只是"看一眼别人的字节" → `string_view`/`Slice`/`(ptr,len)`，零拷贝，但被看的东西必须活得比你久。**
