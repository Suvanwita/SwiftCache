#include "StrlenCommand.h"

#include <string>

namespace swiftcache {

std::string StrlenCommand::name() const {
    return "STRLEN";
}

CommandResult StrlenCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 1) {
        return {"ERR wrong number of arguments for STRLEN\n", false};
    }

    const auto length = store.strlen(args[0]);
    return {std::to_string(length.value_or(0)) + "\n", false};
}

} // namespace swiftcache
