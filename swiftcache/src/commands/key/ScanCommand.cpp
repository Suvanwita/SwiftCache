#include "ScanCommand.h"

#include <sstream>

#include "../../utils/StringUtils.h"

namespace swiftcache {

std::string ScanCommand::name() const {
    return "SCAN";
}

CommandResult ScanCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 1 && args.size() != 3) {
        return {"ERR wrong number of arguments for SCAN\n", false};
    }

    if (args[0] != "0") {
        return {"ERR invalid cursor\n", false};
    }

    std::string pattern = "*";
    if (args.size() == 3) {
        if (toUpper(args[1]) != "MATCH") {
            return {"ERR syntax error\n", false};
        }
        pattern = args[2];
    }

    const auto keys = store.keys(pattern);
    std::ostringstream out;
    out << "0\n";
    for (const auto& key : keys) {
        out << key << "\n";
    }

    return {out.str(), false};
}

} // namespace swiftcache
