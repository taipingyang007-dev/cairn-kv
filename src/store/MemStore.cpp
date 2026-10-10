#include "MemStore.h"
#include <chrono>
#include <cstdint>
#include <optional>

// SET
void MemStore::set(std::string key, std::string value) {
  // 已经存在直接更新，不存在创建
  memStore_[key] = std::move(value);

  // set k v，语义上可以表示清除过期时间
  exDict_.delExpireDictNode(key);
}

// SET
void MemStore::set(std::string key, std::string value,
                   std::int64_t expire_at_ms) {
  // 已经存在直接更新，不存在创建
  memStore_[key] = std::move(value);

  // 保存过期时间
  exDict_.addExpireDictNode(key, expire_at_ms);
}

// GET
std::optional<std::string> MemStore::get(const std::string &key) {
  std::optional<std::string> ans = {};
  auto pos = memStore_.find(key);
  // pos == memStore_.end()，没有找到对应的key
  if (pos == memStore_.end())
    return std::nullopt;

  // 查询过期时间
  auto optimes = exDict_.getExpireDictNode(pos->first);
  if (optimes) {
    // 存在过期时间，判断是不是过期
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    if (now_ms >= *optimes) {
      // 已到期：清理记录，返回不存在
      memStore_.erase(key);
      exDict_.delExpireDictNode(key);
      return std::nullopt;
    } else {
      // 未到期：返回value
      return pos->second;
    }
  }

  // 不存在过期时间直接返回
  return pos->second;
}
// DEL
bool MemStore::del(const std::string &key) {
  if (memStore_.count(key)) {
    // 判断是不是过期，过期理应返回false，但依然需要删除
    // 查询过期时间
    auto optimes = exDict_.getExpireDictNode(key);
    if (optimes) {
      // 存在过期时间，判断是不是过期
      const auto now_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count();
      if (now_ms >= *optimes) {
        // 已到期：清理记录，返回不存在
        memStore_.erase(key);
        exDict_.delExpireDictNode(key);
        return false;
      } else {
        // 未到期：进行清理
        memStore_.erase(key);
        exDict_.delExpireDictNode(key);
        return true;
      }
    } else {
      // key 存在，没有过期时间
      memStore_.erase(key);
      return true;
    }
  }
  // key 不存在
  return false;
}