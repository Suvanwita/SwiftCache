#pragma once

#include <chrono>

#include "../../core/AofPersistence.h"
#include "../../core/Command.h"
#include "../../core/ServerMetrics.h"

namespace swiftcache {

class InfoCommand : public Command {
public:
    InfoCommand(std::chrono::steady_clock::time_point startedAt, const ServerMetrics& metrics,
                const AofPersistence* aof);

    std::string name() const override;
    CommandResult execute(const std::vector<std::string>& args, DataStore& store) override;

private:
    std::chrono::steady_clock::time_point startedAt_;
    const ServerMetrics& metrics_;
    const AofPersistence* aof_;
};

} // namespace swiftcache
