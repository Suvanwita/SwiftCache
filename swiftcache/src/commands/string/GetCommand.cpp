#include "GetCommand.h"

namespace swiftcache {

std::string GetCommand::name() const {
    return "GET";
}

CommandResult GetCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 1) {
        return {"ERR wrong number of arguments for GET\n", false};
    }

    const auto value = store.get(args[0]);
    if (!value.has_value()) {
        return {"(nil)\n", false};
    }

    return {*value + "\n", false};
}

} // namespace swiftcache
