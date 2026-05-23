#include "RenameCommand.h"

namespace swiftcache {

std::string RenameCommand::name() const {
    return "RENAME";
}

CommandResult RenameCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (args.size() != 2) {
        return {"ERR wrong number of arguments for RENAME\n", false};
    }

    if (!store.rename(args[0], args[1])) {
        return {"ERR no such key\n", false};
    }

    return {"OK\n", false};
}

} // namespace swiftcache
