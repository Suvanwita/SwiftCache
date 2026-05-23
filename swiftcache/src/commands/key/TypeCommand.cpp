#include "TypeCommand.h"

namespace swiftcache {

std::string TypeCommand::name() const {
    return "TYPE";
}

CommandResult TypeCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 1) {
        return {"ERR wrong number of arguments for TYPE\n", false};
    }

    return {store.type(args[0]) + "\n", false};
}

} // namespace swiftcache
