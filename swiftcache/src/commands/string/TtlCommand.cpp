#include "TtlCommand.h"

#include <string>

namespace swiftcache {

std::string TtlCommand::name() const {
    return "TTL";
}

CommandResult TtlCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 1) {
        return {"ERR wrong number of arguments for TTL\n", false};
    }

    return {std::to_string(store.ttl(args[0])) + "\n", false};
}

} // namespace swiftcache
