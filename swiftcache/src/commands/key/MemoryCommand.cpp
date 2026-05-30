#include "MemoryCommand.h"

#include "../../utils/StringUtils.h"

namespace swiftcache {

std::string MemoryCommand::name() const {
    return "MEMORY";
}

CommandResult MemoryCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 2 || toUpper(args[0]) != "USAGE") {
        return {"ERR usage: MEMORY USAGE key\n", false};
    }

    const auto usage = store.memoryUsage(args[1]);
    if (!usage.has_value()) {
        return {"(nil)\n", false};
    }

    return {std::to_string(*usage) + "\n", false};
}

} // namespace swiftcache
