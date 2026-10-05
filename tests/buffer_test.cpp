#include "net/Buffer.h"

#include <cstdio>
#include <cstdlib>
#include <string>

// 不用 assert：CMake 默认 Release 带 -DNDEBUG，assert 会整个消失，测试会"假通过"。
// 自己写一个不受 NDEBUG 影响的 CHECK。
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::fprintf(stderr, "CHECK failed: %s  (%s:%d)\n", #cond, __FILE__, __LINE__); \
            std::abort();                                                        \
        }                                                                        \
        ++g_checks;                                                              \
    } while (0)

static int g_checks = 0;

int main()
{
    // 1. 追加 8 字节 → 可读 8
    {
        Buffer b;
        b.append("PING\r\nPI", 8);
        CHECK(b.readable() == 8);
    }

    // 2. 取走 6 → 可读 2，剩下的是 "PI"
    {
        Buffer b;
        b.append("PING\r\nPI", 8);
        b.retrieve(6);
        CHECK(b.readable() == 2);
        CHECK(std::string(b.peek(), b.readable()) == "PI");
    }

    // 3. 追加 3000 字节（超过初始容量）→ 长大且内容完整
    {
        Buffer b;
        std::string big(3000, 'x');
        big[2999] = 'y';
        b.append(big.data(), big.size());
        CHECK(b.readable() == 3000);
        CHECK(std::string(b.peek(), b.readable()) == big);
        CHECK(b.capacity() >= 3000);
    }

    // 4a. 一万次"追加 6、取走 6"（每次全取完）→ 容量不增长
    {
        Buffer b;
        for (int i = 0; i < 10000; ++i) {
            b.append("PING\r\n", 6);
            b.retrieve(6);
        }
        CHECK(b.readable() == 0);
        CHECK(b.capacity() == Buffer::kInitialSize);
    }

    // 4b. 永远留着 2 字节半包，一万次追加/取走 → 靠"挪动"回收，容量仍不增长
    {
        Buffer b;
        b.append("PI", 2);
        for (int i = 0; i < 10000; ++i) {
            b.append("NG\r\nPI", 6);   // 候车室里现在是 "PING\r\nPI"
            b.retrieve(6);             // 领走 "PING\r\n"，剩 "PI"
        }
        CHECK(b.readable() == 2);
        CHECK(std::string(b.peek(), 2) == "PI");
        CHECK(b.capacity() == Buffer::kInitialSize);
    }

    std::printf("buffer_test: all %d checks passed\n", g_checks);
    return 0;
}
