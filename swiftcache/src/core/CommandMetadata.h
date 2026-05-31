#pragma once

#include <string>
#include <vector>

namespace swiftcache {

enum class CommandCategory {
    Read,
    Write,
    Admin,
    PubSub,
    Persistence,
    Client,
    Unknown
};

struct CommandMetadata {
    CommandCategory category{CommandCategory::Unknown};
    bool mutatesData{false};
    bool appendToAof{false};
    bool allowedInReadOnly{true};
};

CommandMetadata commandMetadata(const std::string& commandName);
CommandMetadata commandMetadata(const std::vector<std::string>& tokens);
bool isAofLoggedCommand(const std::vector<std::string>& tokens);
bool isAllowedInReadOnly(const std::vector<std::string>& tokens);

} // namespace swiftcache
