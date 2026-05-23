#include "SismemberCommand.h"

#include <string>

namespace swiftcache {

std::string SismemberCommand::name() const {
    return "SISMEMBER";
}

CommandResult SismemberCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 2) {
        return {"ERR wrong number of arguments for SISMEMBER\n", false};
    }

    const auto isMember = store.sismember(args[0], args[1]);
    if (!isMember.has_value()) {
        return {"ERR wrong type for SISMEMBER\n", false};
    }

    return {std::string(*isMember ? "1\n" : "0\n"), false};
}

} // namespace swiftcache
