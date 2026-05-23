#include "HdelCommand.h"

#include <string>

namespace swiftcache {

std::string HdelCommand::name() const {
    return "HDEL";
}

CommandResult HdelCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 2) {
        return {"ERR wrong number of arguments for HDEL\n", false};
    }

    const auto removed = store.hdel(args[0], args[1]);
    if (!removed.has_value()) {
        return {"ERR wrong type for HDEL\n", false};
    }

    return {std::to_string(*removed) + "\n", false};
}

} // namespace swiftcache
