#include "SetCommand.h"

namespace swiftcache {

std::string SetCommand::name() const {
    return "SET";
}

CommandResult SetCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 2) {
        return {"ERR wrong number of arguments for SET\n", false};
    }

    store.set(args[0], args[1]);
    return {"OK\n", false};
}

} // namespace swiftcache
