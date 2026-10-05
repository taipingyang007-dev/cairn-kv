#include "MemStore.h"

// SET
void MemStore::set(std::string key, std::string value)
{
    // 已经存在直接更新，不存在创建
    memStore_[std::move(key)] = std::move(value);
}
// GET
std::optional<std::string> MemStore::get(const std::string &key)
{
    std::optional<std::string> ans = {};
    auto pos = memStore_.find(key);
    if (pos == memStore_.end())
        return std::nullopt;
    return pos->second;
}
// DEL
bool MemStore::del(const std::string &key)
{
    if (memStore_.count(key))
    {
        memStore_.erase(key);
        return true;
    }
    return false;
}