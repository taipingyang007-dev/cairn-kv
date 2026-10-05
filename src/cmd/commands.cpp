#include "cmd/commands.h"
#include "proto/RespWriter.h"
#include <unordered_map>
#include <algorithm>
using CommandHandler = std::string (*)(const std::vector<std::string> &args, IStore &iStore);
struct Command
{
    int arity;
    CommandHandler handler;
};

// string 小写转大写
static std::string toUpperCopy(std::string s)
{ // 按值传递，结合移动语义
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                   { return static_cast<char>(std::toupper(c)); });
    return s;
}

// redis命令,static约束在文本内
// PING
static std::string ping(const std::vector<std::string> &, IStore &)
{
    return resp::simpleString("PONG");
}

// ECHO
static std::string echo(const std::vector<std::string> &args, IStore &)
{
    return resp::bulkString(args[1]);
}

// COMMAND
static std::string command(const std::vector<std::string> &, IStore &);
// SELECT
static std::string selectDb(const std::vector<std::string> &, IStore &)
{
    return resp::simpleString("OK");
}
// SET
static std::string set(const std::vector<std::string> &args, IStore &iStore)
{
    iStore.set(args[1], args[2]);
    return resp::simpleString("OK");
}

// GET
static std::string get(const std::vector<std::string> &args, IStore &iStore)
{
    std::optional<std::string> res = iStore.get(args[1]);
    return res.has_value() ? resp::bulkString(res.value()) : resp::kNullBulk;
}

// DEL
static std::string del(const std::vector<std::string> &args, IStore &iStore)
{
    return iStore.del(args[1]) ? resp::integer(1) : resp::integer(0);
}

// EXISTS
static std::string exists(const std::vector<std::string> &args, IStore &iStore)
{
    std::optional<std::string> res = iStore.get(args[1]);
    return res.has_value() ? resp::integer(1) : resp::integer(0);
}

static const std::unordered_map<std::string, Command> kCommands = {
    {"PING", {1, ping}},
    {"ECHO", {2, echo}},
    {"COMMAND", {-1, command}},
    {"SELECT", {2, selectDb}},
    {"SET", {-3, set}},
    {"GET", {2, get}},
    {"DEL", {2, del}},
    {"EXISTS", {2, exists}}};

// COMMAND
static std::string command(const std::vector<std::string> &, IStore &)
{
    return resp::kEmptyArray;
}

std::string cmd::handleCommand(const std::vector<std::string> &args, IStore &iStore)
{
    std::string arg1 = toUpperCopy(args[0]);
    // 查询命令
    if (!kCommands.contains(arg1))
        return resp::error("ERR unknown command '" + args[0] + "'");

    // 检查参数
    size_t argc = kCommands.at(arg1).arity > 0 ? kCommands.at(arg1).arity : -kCommands.at(arg1).arity;
    if (kCommands.at(arg1).arity > 0)
    {
        if (argc != args.size())
        {
            return resp::error("ERR wrong number of arguments for '" + arg1 + "' command");
        }
    }
    else
    {
        if (argc > args.size())
        {
            return resp::error("ERR wrong number of arguments for '" + arg1 + "' command");
        }
    }

    // 返回处理结果
    return kCommands.at(arg1).handler(args, iStore);
}