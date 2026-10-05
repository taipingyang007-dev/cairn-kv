#pragma once
#include <string>
#include <string_view>

namespace resp
{
    // 简单字符串
    std::string simpleString(std::string_view s);   // +s\r\n
    // 错误
    std::string error(std::string_view s);     // -s\r\n
    // 整数
    std::string integer(long long n);          // :n\r\n
    // 批量字符串
    std::string bulkString(std::string_view s);     // $len\r\n s \r\n
    // 空：Null——key 不存在
    constexpr const char *kNullBulk = "$-1\r\n";    // $-1\r\n
    // 空数组
    constexpr const char *kEmptyArray = "*0\r\n";   // *0\r\n
}