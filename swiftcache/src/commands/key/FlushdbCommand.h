#pragma once

#include "../../core/Command.h"

namespace swiftcache {

class FlushdbCommand : public Command {
public:
    std::string name() const override;
    CommandResult execute(const std::vector<std::string>& args, DataStore& store) override;
};

} // namespace swiftcache
