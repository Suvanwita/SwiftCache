#include "FlushdbCommand.h"

namespace swiftcache {

std::string FlushdbCommand::name() const {
    return "FLUSHDB";
}

CommandResult FlushdbCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (!args.empty()) {
        return {"ERR wrong number of arguments for FLUSHDB\n", false};
    }

    store.flushdb();
    return {"OK\n", false};
}

} // namespace swiftcache
