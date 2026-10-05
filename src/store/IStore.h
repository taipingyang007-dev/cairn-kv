#pragma once
#include <string>
#include <optional>
class IStore
{
public:
    IStore() = default;
    virtual ~IStore() = default;

    virtual void set(std::string key, std::string value) = 0;
    virtual std::optional<std::string> get(const std::string &key) = 0;
    virtual bool del(const std::string &key) = 0;
};