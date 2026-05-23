#include "AppendCommand.h"

#include <string>

namespace swiftcache {

std::string AppendCommand::name() const {
    return "APPEND";
}

CommandResult AppendCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 2) {
        return {"ERR wrong number of arguments for APPEND\n", false};
    }

    const auto length = store.append(args[0], args[1]);
    if (!length.has_value()) {
        return {"ERR wrong type for APPEND\n", false};
    }

    return {std::to_string(*length) + "\n", false};
}

} // namespace swiftcache
