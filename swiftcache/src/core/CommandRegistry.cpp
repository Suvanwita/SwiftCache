#include "CommandRegistry.h"

#include "../utils/StringUtils.h"

namespace swiftcache {

void CommandRegistry::registerCommand(CommandPtr command) {
    if (!command) {
        return;
    }
    commands_[toUpper(command->name())] = std::move(command);
}

CommandResult CommandRegistry::execute(const std::vector<std::string>& tokens, DataStore& store) const {
    if (tokens.empty()) {
        return {"ERR empty command\n", false};
    }

    const auto commandName = toUpper(tokens.front());
    const auto it = commands_.find(commandName);
    if (it == commands_.end()) {
        return {"ERR unknown command '" + tokens.front() + "'\n", false};
    }

    std::vector<std::string> args(tokens.begin() + 1, tokens.end());
    return it->second->execute(args, store);
}

} // namespace swiftcache
