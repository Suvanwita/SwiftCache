#include "HgetCommand.h"

namespace swiftcache {

std::string HgetCommand::name() const {
    return "HGET";
}

CommandResult HgetCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 2) {
        return {"ERR wrong number of arguments for HGET\n", false};
    }

    const auto result = store.hget(args[0], args[1]);
    if (result.status == DataStoreStatus::WrongType) {
        return {"ERR wrong type for HGET\n", false};
    }
    if (!result.value.has_value()) {
        return {"(nil)\n", false};
    }

    return {*result.value + "\n", false};
}

} // namespace swiftcache
