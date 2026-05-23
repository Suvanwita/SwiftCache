#include "HexistsCommand.h"

#include <string>

namespace swiftcache {

std::string HexistsCommand::name() const {
    return "HEXISTS";
}

CommandResult HexistsCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 2) {
        return {"ERR wrong number of arguments for HEXISTS\n", false};
    }

    const auto exists = store.hexists(args[0], args[1]);
    if (!exists.has_value()) {
        return {"ERR wrong type for HEXISTS\n", false};
    }

    return {std::string(*exists ? "1\n" : "0\n"), false};
}

} // namespace swiftcache
