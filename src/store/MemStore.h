#pragma once
#include <unordered_map>
#include <string>
#include <optional>
#include "IStore.h"

class MemStore : public IStore
{
public:
    // SET
    void set(std::string key, std::string value) override;
    // GET
    std::optional<std::string> get(const std::string &key) override;
    // DEL
    bool del(const std::string &key) override;

private:
    std::unordered_map<std::string, std::string> memStore_ = {};
};