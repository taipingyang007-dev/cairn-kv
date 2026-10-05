#include "RespWriter.h"

// 简单字符串
std::string resp::simpleString(std::string_view s) { return "+" + std::string(s) + "\r\n"; } // +s\r\n
// 错误
std::string resp::error(std::string_view s) { return "-" + std::string(s) + "\r\n"; } // -s\r\n
// 整数
std::string resp::integer(long long n) { return ":" + std::to_string(n) + "\r\n"; } // :n\r\n
// 批量字符串
std::string resp::bulkString(std::string_view s) { return "$" + std::to_string(s.size()) + "\r\n" + std::string(s) + "\r\n"; } // $len\r\n s \r\n