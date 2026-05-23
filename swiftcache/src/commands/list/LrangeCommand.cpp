#include "LrangeCommand.h"

#include <exception>
#include <sstream>
#include <string>

namespace swiftcache {
namespace {

bool parseIndex(const std::string& value, long long& parsed) {
    try {
        std::size_t consumed = 0;
        parsed = std::stoll(value, &consumed);
        return consumed == value.size();
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace

std::string LrangeCommand::name() const {
    return "LRANGE";
}

CommandResult LrangeCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 3) {
        return {"ERR wrong number of arguments for LRANGE\n", false};
    }

    long long start = 0;
    long long stop = 0;
    if (!parseIndex(args[1], start) || !parseIndex(args[2], stop)) {
        return {"ERR invalid range index\n", false};
    }

    const auto result = store.lrange(args[0], start, stop);
    if (result.status == DataStoreStatus::WrongType) {
        return {"ERR wrong type for LRANGE\n", false};
    }

    std::ostringstream out;
    for (const auto& value : result.values) {
        out << value << "\n";
    }
    return {out.str(), false};
}

} // namespace swiftcache
