#pragma once
#include <string>
#include <vector>
#include "store/IStore.h"
namespace cmd
{
    std::string handleCommand(const std::vector<std::string> &args, IStore &iStore);
}