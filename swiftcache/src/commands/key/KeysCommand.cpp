#include "KeysCommand.h"

#include <sstream>

namespace swiftcache {

std::string KeysCommand::name() const {
    return "KEYS";
}

CommandResult KeysCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() > 1) {
        return {"ERR wrong number of arguments for KEYS\n", false};
    }

    const auto keys = store.keys(args.empty() ? "*" : args[0]);
    std::ostringstream out;
    for (const auto& key : keys) {
        out << key << "\n";
    }
    return {out.str(), false};
}

} // namespace swiftcache
