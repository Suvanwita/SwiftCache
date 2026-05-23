#include "LpopCommand.h"

namespace swiftcache {

std::string LpopCommand::name() const {
    return "LPOP";
}

CommandResult LpopCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 1) {
        return {"ERR wrong number of arguments for LPOP\n", false};
    }

    const auto result = store.lpop(args[0]);
    if (result.status == DataStoreStatus::WrongType) {
        return {"ERR wrong type for LPOP\n", false};
    }
    if (result.status == DataStoreStatus::Missing) {
        return {"(nil)\n", false};
    }

    return {result.value + "\n", false};
}

} // namespace swiftcache
