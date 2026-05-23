#include "LpushCommand.h"

#include <string>
#include <vector>

namespace swiftcache {

std::string LpushCommand::name() const {
    return "LPUSH";
}

CommandResult LpushCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() < 2) {
        return {"ERR wrong number of arguments for LPUSH\n", false};
    }

    const std::vector<std::string> values(args.begin() + 1, args.end());
    const auto size = store.lpush(args[0], values);
    if (!size.has_value()) {
        return {"ERR wrong type for LPUSH\n", false};
    }

    return {std::to_string(*size) + "\n", false};
}

} // namespace swiftcache
