#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Command.h"

namespace swiftcache {

class CommandRegistry {
public:
    void registerCommand(CommandPtr command);
    CommandResult execute(const std::vector<std::string>& tokens, DataStore& store) const;

private:
    std::unordered_map<std::string, CommandPtr> commands_;
};

} // namespace swiftcache
