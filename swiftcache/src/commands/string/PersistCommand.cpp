#include "PersistCommand.h"

#include <string>

namespace swiftcache {

std::string PersistCommand::name() const {
    return "PERSIST";
}

CommandResult PersistCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 1) {
        return {"ERR wrong number of arguments for PERSIST\n", false};
    }

    return {std::string(store.persist(args[0]) ? "1\n" : "0\n"), false};
}

} // namespace swiftcache
