#include "RocksDBStore.h"
#include <rocksdb/db.h>

RocksDBStore::~RocksDBStore() { close(); }
void RocksDBStore::close() {
  if (db_ != nullptr) {
    delete db_;
    db_ = nullptr;
  }
}

RocksDBStore::RocksDBStore(const char *path) {
  rocksdb::Options opts;
  opts.create_if_missing = true; // 目录不存在就建，不设这个第一次跑必失败
  rocksdb::Status s = rocksdb::DB::Open(
      opts, path, &db_); // 成功后 db 指向堆上的对象，归你 delete
  if (!s.ok()) {
    fprintf(stderr, "RocksDBStore open failed: %s\n", s.ToString().c_str());
    abort();
  }
}
void RocksDBStore::set(std::string key, std::string value) {
  rocksdb::Status s = db_->Put(rocksdb::WriteOptions(), key, value);
  if (!s.ok())
    fprintf(stderr, "RocksDBStore put failed: %s\n", s.ToString().c_str());
}

void RocksDBStore::set(std::string key, std::string value,
                       std::int64_t expire_at_ms) {}
                       
std::optional<std::string> RocksDBStore::get(const std::string &key) {
  std::string value = {};
  rocksdb::Status s = db_->Get(rocksdb::ReadOptions(), key, &value);
  if (s.ok())
    return value;

  if (!s.IsNotFound()) {
    // error
    fprintf(stderr, "RocksDBStore get failed: %s\n", s.ToString().c_str());
  }

  return std::nullopt;
}

bool RocksDBStore::del(const std::string &key) {
  std::optional<std::string> ans = get(key);

  if (!ans.has_value()) {
    // NotFound或者get error
    return false;
  }

  rocksdb::Status s = db_->Delete(rocksdb::WriteOptions(), key);
  if (!s.ok()) {
    fprintf(stderr, "RocksDBStore del failed: %s\n", s.ToString().c_str());
    return false;
  }
  return true;
}