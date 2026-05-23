#include "DelCommand.h"

namespace swiftcache {

std::string DelCommand::name() const {
    return "DEL";
}

CommandResult DelCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 1) {
        return {"ERR wrong number of arguments for DEL\n", false};
    }

    return {std::string(store.del(args[0]) ? "1\n" : "0\n"), false};
}

} // namespace swiftcache
