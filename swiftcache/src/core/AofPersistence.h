#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "Command.h"
#include "CommandRegistry.h"
#include "../datastore/DataStore.h"

namespace swiftcache {

class AofPersistence {
public:
    explicit AofPersistence(std::string path);

    bool replay(const CommandRegistry& registry, DataStore& store) const;
    bool append(const std::vector<std::string>& tokens);
    CommandResult executeAndAppend(const std::vector<std::string>& tokens,
                                   const std::function<CommandResult()>& execute,
                                   bool& appendSucceeded);
    bool checkpoint(const std::function<bool()>& saveSnapshot);
    bool isMutatingCommand(const std::vector<std::string>& tokens) const;

    const std::string& path() const;

private:
    static std::string encodeRespCommand(const std::vector<std::string>& tokens);

    std::string path_;
    mutable std::mutex mutex_;
};

} // namespace swiftcache
