#include "SmembersCommand.h"

#include <sstream>

namespace swiftcache {

std::string SmembersCommand::name() const {
    return "SMEMBERS";
}

CommandResult SmembersCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 1) {
        return {"ERR wrong number of arguments for SMEMBERS\n", false};
    }

    const auto result = store.smembers(args[0]);
    if (result.status == DataStoreStatus::WrongType) {
        return {"ERR wrong type for SMEMBERS\n", false};
    }

    std::ostringstream out;
    for (const auto& member : result.members) {
        out << member << "\n";
    }

    return {out.str(), false};
}

} // namespace swiftcache
