#pragma once

#include <vector>
#include <string>
#include <string_view>

namespace resp
{
    enum class ParseStatus
    {
        OK,
        NeedMore,
        Error
    };

    struct ParseResult
    {
        ParseStatus status;
        std::vector<std::string> args = {};
        size_t consumed = 0;
    };

    // EASP格式解析
    ParseResult parse(std::string_view in);
}