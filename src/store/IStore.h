#pragma once
#include <cstdint>
#include <optional>
#include <string>
class IStore {
public:
  IStore() = default;
  virtual ~IStore() = default;

  virtual void set(std::string key, std::string value) = 0;
  virtual void set(std::string key, std::string value, std::int64_t expire_at_ms) = 0;
  virtual std::optional<std::string> get(const std::string &key) = 0;
  virtual bool del(const std::string &key) = 0;
};