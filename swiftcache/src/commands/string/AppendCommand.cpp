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

    return {std::to_string(store.append(args[0], args[1])) + "\n", false};
}

} // namespace swiftcache
