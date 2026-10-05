#include "RespParser.h"
#include <stdio.h>
#include <charconv>
resp::ParseResult resp::parse(std::string_view in)
{
    resp::ParseResult ans; // 记录parse的结果

    // 数据为空
    if (in.empty())
    {
        ans.status = resp::ParseStatus::NeedMore;
        return ans;
    }

    // 首个元素非 '*'，判断为格式错误
    if (in[0] != '*')
    {
        ans.status = resp::ParseStatus::Error;
        return ans;
    }

    // 解析数组元素数量 （*<count>\r\n）
    size_t first_crlf = in.find("\r\n");
    if (first_crlf == std::string_view::npos)
    {
        ans.status = resp::ParseStatus::NeedMore;
        return ans;
    }

    int count = {};
    auto res = std::from_chars(in.data() + 1, in.data() + first_crlf, count);
    if (res.ec != std::errc{} || count <= 0)
    {
        ans.status = resp::ParseStatus::Error;
        return ans;
    }

    size_t cursor = first_crlf + 2; // 游标，标记住每一个起始的 '$'

    ans.args.reserve(count); // 预分配空间
    for (int i = 0; i < count; ++i)
    {
        if (cursor >= in.size())
        {
            return {resp::ParseStatus::NeedMore, {}, 0};
        }

        // 格式错误
        if (in[cursor] != '$')
        {
            return {resp::ParseStatus::Error, {}, 0};
        }

        // 定位长度行的结尾 \r\n
        size_t len_crlf = in.find("\r\n", cursor);
        if (len_crlf == std::string_view::npos)
        {
            return {resp::ParseStatus::NeedMore, {}, 0};
        }

        // 解析字符串长度
        int str_len = {};
        res = std::from_chars(in.data() + cursor + 1, in.data() + len_crlf, str_len);
        if (res.ec != std::errc{} || str_len < 0)
        {
            return {resp::ParseStatus::Error, {}, 0};
        }

        // 定位真实数据区间
        size_t data_start = len_crlf + 2;
        size_t data_end = len_crlf + 2 + str_len;

        if (data_end + 2 > in.size())
        {
            return {resp::ParseStatus::NeedMore, {}, 0};
        }

        // 尾部是不是符合 "\r\n"
        if (in[data_end] != '\r' || in[data_end + 1] != '\n')
        {
            return {resp::ParseStatus::Error, {}, 0};
        }

        ans.args.emplace_back(in.substr(data_start, str_len));

        // 推进游标，准备处理下一个参数
        cursor = data_end + 2;
    }

    ans.status = resp::ParseStatus::OK;
    ans.consumed = cursor;
    return ans;
}