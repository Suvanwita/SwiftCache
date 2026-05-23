#include "SaddCommand.h"

#include <string>
#include <vector>

namespace swiftcache {

std::string SaddCommand::name() const {
    return "SADD";
}

CommandResult SaddCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() < 2) {
        return {"ERR wrong number of arguments for SADD\n", false};
    }

    const std::vector<std::string> members(args.begin() + 1, args.end());
    const auto added = store.sadd(args[0], members);
    if (!added.has_value()) {
        return {"ERR wrong type for SADD\n", false};
    }

    return {std::to_string(*added) + "\n", false};
}

} // namespace swiftcache
