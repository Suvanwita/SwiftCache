#include "DecrCommand.h"

#include <string>

namespace swiftcache {

std::string DecrCommand::name() const {
    return "DECR";
}

CommandResult DecrCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 1) {
        return {"ERR wrong number of arguments for DECR\n", false};
    }

    const auto value = store.incrBy(args[0], -1);
    if (!value.has_value()) {
        return {"ERR value is not an integer\n", false};
    }

    return {std::to_string(*value) + "\n", false};
}

} // namespace swiftcache
