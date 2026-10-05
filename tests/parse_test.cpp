#include "proto/RespParser.h"
#include <cstdio>
#include <cstdlib>

#define CHECK(cond)                                                                        \
    do                                                                                     \
    {                                                                                      \
        if (!(cond))                                                                       \
        {                                                                                  \
            std::fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
            std::abort();                                                                  \
        }                                                                                  \
        ++g_checks;                                                                        \
    } while (0)

static size_t g_checks = {};

/*
完整 PING → OK, consumed=14, ["PING"]
完整 SET  → OK, consumed=35, ["SET", "name", "jixing"]
半包      → NeedMore, consumed=0        ← 一个字节都不动，对
粘包两条  → OK, consumed=14, ["PING"]   ← 只解第一条，剩下的留给下一轮，对
把 SET 那条切成两半 -> 解析两次 -> OK
*1\r\n$6\r\nab\r\n（长度撒谎）-> status == Error
*/

int main()
{
    // 完整 PING → OK, consumed=14, ["PING"]
    {
        auto res = resp::parse("*1\r\n$4\r\nPING\r\n");
        CHECK(res.status == resp::ParseStatus::OK);
        CHECK(res.args.size() == 1);
        CHECK(res.consumed == (sizeof("*1\r\n$4\r\nPING\r\n") - 1));
        CHECK(res.args[0] == "PING");
    }

    // 完整 SET  → OK, consumed=35, ["SET", "name", "jixing"]
    {
        auto res = resp::parse("*3\r\n$3\r\nSET\r\n$4\r\nname\r\n$6\r\njixing\r\n");
        CHECK(res.status == resp::ParseStatus::OK);
        CHECK(res.args.size() == 3);
        CHECK(res.consumed == sizeof("*3\r\n$3\r\nSET\r\n$4\r\nname\r\n$6\r\njixing\r\n") - 1);
        CHECK(res.args[0] == "SET");
        CHECK(res.args[1] == "name");
        CHECK(res.args[2] == "jixing");
    }

    //  半包      → NeedMore, consumed=0        ← 一个字节都不动，对
    {
        auto res = resp::parse("*3\r\n$3\r\nSET\r\n$4\r\n");
        CHECK(res.status == resp::ParseStatus::NeedMore);
        CHECK(res.args.empty());
        CHECK(res.consumed == 0); // 理应等于0，应该只有读取完整的命令之后才会设置

        // 还有就是关于一个字节都不动，其实我没都没有删除这样的操作，只是调用哪个“已读”函数，我暂时忘了是什么了，但是现在应该还不能用
    }

    // 粘包两条  → OK, consumed=14, ["PING"]   ← 只解第一条，剩下的留给下一轮，对
    {
        auto res = resp::parse("*1\r\n$4\r\nPING\r\n*3\r\n$3\r\nSET\r\n$4\r\nname\r\n$6\r\njixing\r\n");
        CHECK(res.status == resp::ParseStatus::OK);
        CHECK(res.consumed == (sizeof("*1\r\n$4\r\nPING\r\n") - 1));
        CHECK(res.args.size() == 1);
        CHECK(res.args[0] == "PING");
    }

    // 把 SET 那条切成两半 -> 解析两次 -> OK
    {
        std::string_view full = "*3\r\n$3\r\nSET\r\n$4\r\nname\r\n$6\r\njixing\r\n";
        CHECK(resp::parse(full.substr(0, 20)).status == resp::ParseStatus::NeedMore); // 前半 → 不够
        auto r = resp::parse(full);                                             // 补齐 → 和一次喂一样
        CHECK(r.status == resp::ParseStatus::OK);
        CHECK(r.args.size() == 3);
    }
    // 长度小于实际数据 → Error（说好 2 字节，第 3 位该是 \r 却是 'c'，再来多少字节也救不回来）
    {
        auto res = resp::parse("*1\r\n$2\r\nabc\r\n");
        CHECK(res.status == resp::ParseStatus::Error);
        CHECK(res.args.empty());
        CHECK(res.consumed == 0);
    }

    // 首字节不是 '*' → Error（命令必须是数组）
    {
        auto res = resp::parse("+OK\r\n");
        CHECK(res.status == resp::ParseStatus::Error);
        CHECK(res.args.empty());
    }

    // 长度大于实际数据 → NeedMore，不是 Error
    // 流式协议里"撒谎"和"还没发完"无法区分，只能继续等
    {
        auto res = resp::parse("*1\r\n$6\r\nab\r\n");
        CHECK(res.status == resp::ParseStatus::NeedMore);
        CHECK(res.consumed == 0);
    }

    std::printf("parse_test: all %zu checks passed\n", g_checks);

    return 0;
}