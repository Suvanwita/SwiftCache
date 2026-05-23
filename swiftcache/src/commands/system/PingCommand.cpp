#include "PingCommand.h"

namespace swiftcache {

std::string PingCommand::name() const {
    return "PING";
}

CommandResult PingCommand::execute(const std::vector<std::string>& args, DataStore&) {
    if (!args.empty()) {
        return {"ERR wrong number of arguments for PING\n", false};
    }

    return {"PONG\n", false};
}

} // namespace swiftcache
