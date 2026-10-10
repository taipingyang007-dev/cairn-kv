#pragma once
#include "IStore.h"

namespace rocksdb {
class DB;
} // namespace rocksdb
class RocksDBStore : public IStore {
public:
  RocksDBStore(const char *path);
  ~RocksDBStore();

  RocksDBStore(const RocksDBStore &) = delete;
  RocksDBStore &operator=(const RocksDBStore &) = delete;

  void set(std::string key, std::string value) override;
  void set(std::string key, std::string value,
           std::int64_t expire_at_ms) override;
  std::optional<std::string> get(const std::string &key) override;
  bool del(const std::string &key) override;

private:
  void close();
  rocksdb::DB *db_ = nullptr;
};