#include "ExistsCommand.h"

namespace swiftcache {

std::string ExistsCommand::name() const {
    return "EXISTS";
}

CommandResult ExistsCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 1) {
        return {"ERR wrong number of arguments for EXISTS\n", false};
    }

    return {std::string(store.exists(args[0]) ? "1\n" : "0\n"), false};
}

} // namespace swiftcache
