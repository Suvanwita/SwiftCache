#include "SremCommand.h"

#include <string>
#include <vector>

namespace swiftcache {

std::string SremCommand::name() const {
    return "SREM";
}

CommandResult SremCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() < 2) {
        return {"ERR wrong number of arguments for SREM\n", false};
    }

    const std::vector<std::string> members(args.begin() + 1, args.end());
    const auto removed = store.srem(args[0], members);
    if (!removed.has_value()) {
        return {"ERR wrong type for SREM\n", false};
    }

    return {std::to_string(*removed) + "\n", false};
}

} // namespace swiftcache
