#include "HsetCommand.h"

#include <string>

namespace swiftcache {

std::string HsetCommand::name() const {
    return "HSET";
}

CommandResult HsetCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 3) {
        return {"ERR wrong number of arguments for HSET\n", false};
    }

    const auto updated = store.hset(args[0], args[1], args[2]);
    if (!updated.has_value()) {
        return {"ERR wrong type for HSET\n", false};
    }

    return {std::to_string(*updated) + "\n", false};
}

} // namespace swiftcache
