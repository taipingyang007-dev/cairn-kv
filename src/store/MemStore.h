#pragma once
#include "ExpireDict.h"
#include "IStore.h"
#include <optional>
#include <string>
#include <unordered_map>

class MemStore : public IStore {
public:
  // SET
  void set(std::string key, std::string value) override;
  void set(std::string key, std::string value, std::int64_t expire_at_ms) override;

  // GET
  std::optional<std::string> get(const std::string &key) override;
  // DEL
  bool del(const std::string &key) override;

private:
  // 存储 key - value
  std::unordered_map<std::string, std::string> memStore_ = {};
  // 存储 key - 过期时间
  ExpireDict exDict_;
};