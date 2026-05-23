#include "MsetCommand.h"

#include <utility>
#include <vector>

namespace swiftcache {

std::string MsetCommand::name() const {
    return "MSET";
}

CommandResult MsetCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.empty() || args.size() % 2 != 0) {
        return {"ERR wrong number of arguments for MSET\n", false};
    }

    std::vector<std::pair<std::string, std::string>> entries;
    entries.reserve(args.size() / 2);
    for (std::size_t i = 0; i < args.size(); i += 2) {
        entries.emplace_back(args[i], args[i + 1]);
    }

    store.mset(entries);
    return {"OK\n", false};
}

} // namespace swiftcache
