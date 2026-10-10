#include "store/ExpireDict.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>

static int g_checks{};

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "CHECK failed: %s  (%s:%d)\n", #cond, __FILE__,     \
                   __LINE__);                                                  \
      std::abort();                                                            \
    }                                                                          \
    ++g_checks;                                                                \
  } while (0)

int main() {
  CHECK(std::hash<std::string>{}("a") % 4 == std::hash<std::string>{}("f") % 4);

  // 添加、更新、删除查询
  ExpireDict exDict;
  {
    // 添加
    exDict.addExpireDictNode("a", 10);
    // 查询
    auto result = exDict.getExpireDictNode("a");
    CHECK(result.has_value());
    CHECK(*result == 10);

    // 更新
    exDict.addExpireDictNode("a", 1000);

    // 查询
    result = exDict.getExpireDictNode("a");
    CHECK(result.has_value());
    CHECK(*result == 1000);

    // 删除
    bool flags = exDict.delExpireDictNode("a");
    CHECK(flags);
    // 查询
    result = exDict.getExpireDictNode("a");
    CHECK(!result.has_value());
  }

  {
    // 添加
    exDict.addExpireDictNode("a", 10);
    exDict.addExpireDictNode("f", 20);
    // 查询 a
    auto result = exDict.getExpireDictNode("a");
    CHECK(result.has_value());
    CHECK(*result == 10);

    // 更新 a
    exDict.addExpireDictNode("a", 1000);

    // 查询 f
    result = exDict.getExpireDictNode("f");
    CHECK(result.has_value());
    CHECK(*result == 20);

    // 更新 f
    exDict.addExpireDictNode("f", 1000);

    // 查询 a
    result = exDict.getExpireDictNode("a");
    CHECK(result.has_value());
    CHECK(*result == 1000);

    // 查询 f
    result = exDict.getExpireDictNode("f");
    CHECK(result.has_value());
    CHECK(*result == 1000);

    // 删除 a
    bool flags = exDict.delExpireDictNode("a");
    CHECK(flags);

    // 查询 a
    result = exDict.getExpireDictNode("a");
    CHECK(!result.has_value());

    // 查询 f
    result = exDict.getExpireDictNode("f");
    CHECK(result.has_value());
    CHECK(*result == 1000);

    // 删除 f
    flags = exDict.delExpireDictNode("f");
    CHECK(flags);

    // 查询 f
    result = exDict.getExpireDictNode("f");
    CHECK(!result.has_value());
  }

  {
    // 添加
    exDict.addExpireDictNode("", 10);
    
    // 查询 ""
    auto result = exDict.getExpireDictNode("");
    CHECK(result.has_value());
    CHECK(*result == 10);

    // 更新 a
    exDict.addExpireDictNode("", 1000);

    // 查询 ""
    result = exDict.getExpireDictNode("");
    CHECK(result.has_value());
    CHECK(*result == 1000);
    
    // 删除 f
    auto flags = exDict.delExpireDictNode("");
    CHECK(flags);
    CHECK(!exDict.getExpireDictNode("").has_value());
  }

  {
    // 三个 key 落在同一桶，删除中间节点后，两端仍应保持原值。
    const auto bucket = std::hash<std::string>{}("a") % 4;
    std::string third;
    for (int i = 0; i < 100 && third.empty(); ++i) {
      std::string candidate = "collision_" + std::to_string(i);
      if (std::hash<std::string>{}(candidate) % 4 == bucket) {
        third = candidate;
      }
    }
    CHECK(!third.empty());

    ExpireDict dict;
    dict.addExpireDictNode("a", 10);
    dict.addExpireDictNode("f", 20);
    dict.addExpireDictNode(third, 30);

    CHECK(dict.delExpireDictNode("f"));
    CHECK(!dict.getExpireDictNode("f").has_value());

    auto first = dict.getExpireDictNode("a");
    auto last = dict.getExpireDictNode(third);
    CHECK(first.has_value());
    CHECK(*first == 10);
    CHECK(last.has_value());
    CHECK(*last == 30);
  }
}
