#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../datastore/DataStore.h"

namespace swiftcache {

struct CommandResult {
    std::string response;
    bool closeConnection{false};
};

class Command {
public:
    virtual ~Command() = default;
    virtual std::string name() const = 0;
    virtual CommandResult execute(const std::vector<std::string>& args, DataStore& store) = 0;
};

using CommandPtr = std::unique_ptr<Command>;

} // namespace swiftcache
