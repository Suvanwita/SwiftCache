#include "ExpireCommand.h"

#include <exception>
#include <string>

namespace swiftcache {

std::string ExpireCommand::name() const {
    return "EXPIRE";
}

CommandResult ExpireCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 2) {
        return {"ERR wrong number of arguments for EXPIRE\n", false};
    }

    long long ttlSeconds = 0;
    try {
        std::size_t parsed = 0;
        ttlSeconds = std::stoll(args[1], &parsed);
        if (parsed != args[1].size()) {
            return {"ERR invalid expire time\n", false};
        }
    } catch (const std::exception&) {
        return {"ERR invalid expire time\n", false};
    }

    if (ttlSeconds < 0) {
        return {"ERR invalid expire time\n", false};
    }

    return {std::string(store.expire(args[0], ttlSeconds) ? "1\n" : "0\n"), false};
}

} // namespace swiftcache
