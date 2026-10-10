#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

using times = std::int64_t; // 使用 int64_t 来表示时间

// 自定义数据结构，仿哈希表并完成自定义需求，作用主要在于过期处理
class ExpireDict {
  // 哈希表的节点
  struct ExpireDictNode {
    // key
    std::string key{};
    // expire_at_ms
    times expire_at_ms{};
    // next node，使用链表来处理哈希冲突
    ExpireDictNode *next{};
  };

public:
  ExpireDict();
  ~ExpireDict();

  ExpireDict(const ExpireDict &) = delete;
  ExpireDict &operator=(const ExpireDict &) = delete;

  // 添加/更新 ExpireDictNode，添加的方式是{key - times}
  void addExpireDictNode(std::string key, times expire_ms);

  // 删除
  bool delExpireDictNode(std::string key);

  // 查询
  std::optional<times> getExpireDictNode(std::string key);

private:
  void reset(); // reset 重置 ExpireArr_
  // 返回查询结果，如果存在{true，目标节点的前驱}，不存在{false，最后一个节点}
  std::pair<bool, ExpireDictNode *> findPrev(const std::string &key);
  std::array<ExpireDictNode *, 4> ExpireArr_;
};