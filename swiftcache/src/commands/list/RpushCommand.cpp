#include "RpushCommand.h"

#include <string>
#include <vector>

namespace swiftcache {

std::string RpushCommand::name() const {
    return "RPUSH";
}

CommandResult RpushCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() < 2) {
        return {"ERR wrong number of arguments for RPUSH\n", false};
    }

    const std::vector<std::string> values(args.begin() + 1, args.end());
    const auto size = store.rpush(args[0], values);
    if (!size.has_value()) {
        return {"ERR wrong type for RPUSH\n", false};
    }

    return {std::to_string(*size) + "\n", false};
}

} // namespace swiftcache
