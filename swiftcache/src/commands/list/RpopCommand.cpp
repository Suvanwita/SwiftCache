#include "RpopCommand.h"

namespace swiftcache {

std::string RpopCommand::name() const {
    return "RPOP";
}

CommandResult RpopCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 1) {
        return {"ERR wrong number of arguments for RPOP\n", false};
    }

    const auto result = store.rpop(args[0]);
    if (result.status == DataStoreStatus::WrongType) {
        return {"ERR wrong type for RPOP\n", false};
    }
    if (result.status == DataStoreStatus::Missing) {
        return {"(nil)\n", false};
    }

    return {result.value + "\n", false};
}

} // namespace swiftcache
