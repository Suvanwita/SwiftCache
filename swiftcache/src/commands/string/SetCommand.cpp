#include "SetCommand.h"

#include <stdexcept>

#include "../../utils/StringUtils.h"

namespace swiftcache {

std::string SetCommand::name() const {
    return "SET";
}

CommandResult SetCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 2 && args.size() != 4) {
        return {"ERR wrong number of arguments for SET\n", false};
    }

    long long ttlSeconds = -1;
    if (args.size() == 4) {
        if (toUpper(args[2]) != "EX") {
            return {"ERR syntax error\n", false};
        }

        try {
            std::size_t parsed = 0;
            ttlSeconds = std::stoll(args[3], &parsed);
            if (parsed != args[3].size()) {
                return {"ERR invalid expire time\n", false};
            }
        } catch (const std::exception&) {
            return {"ERR invalid expire time\n", false};
        }

        if (ttlSeconds < 0) {
            return {"ERR invalid expire time\n", false};
        }
    }

    store.set(args[0], args[1], ttlSeconds);
    return {"OK\n", false};
}

} // namespace swiftcache
