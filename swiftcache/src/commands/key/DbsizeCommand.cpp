#include "DbsizeCommand.h"

namespace swiftcache {

std::string DbsizeCommand::name() const {
    return "DBSIZE";
}

CommandResult DbsizeCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (!args.empty()) {
        return {"ERR wrong number of arguments for DBSIZE\n", false};
    }

    return {std::to_string(store.dbsize()) + "\n", false};
}

} // namespace swiftcache
