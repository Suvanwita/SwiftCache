#include "MgetCommand.h"

#include <sstream>

namespace swiftcache {

std::string MgetCommand::name() const {
    return "MGET";
}

CommandResult MgetCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.empty()) {
        return {"ERR wrong number of arguments for MGET\n", false};
    }

    const auto values = store.mget(args);
    std::ostringstream out;
    for (const auto& value : values) {
        out << (value.has_value() ? *value : "(nil)") << "\n";
    }

    return {out.str(), false};
}

} // namespace swiftcache
