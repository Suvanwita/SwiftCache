#include "ScardCommand.h"

#include <string>

namespace swiftcache {

std::string ScardCommand::name() const {
    return "SCARD";
}

CommandResult ScardCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 1) {
        return {"ERR wrong number of arguments for SCARD\n", false};
    }

    const auto count = store.scard(args[0]);
    if (!count.has_value()) {
        return {"ERR wrong type for SCARD\n", false};
    }

    return {std::to_string(*count) + "\n", false};
}

} // namespace swiftcache
