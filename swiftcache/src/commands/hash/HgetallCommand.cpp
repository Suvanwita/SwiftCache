#include "HgetallCommand.h"

#include <sstream>

namespace swiftcache {

std::string HgetallCommand::name() const {
    return "HGETALL";
}

CommandResult HgetallCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 1) {
        return {"ERR wrong number of arguments for HGETALL\n", false};
    }

    const auto result = store.hgetall(args[0]);
    if (result.status == DataStoreStatus::WrongType) {
        return {"ERR wrong type for HGETALL\n", false};
    }

    std::ostringstream out;
    for (const auto& field : result.fields) {
        out << field.first << "\n" << field.second << "\n";
    }

    return {out.str(), false};
}

} // namespace swiftcache
