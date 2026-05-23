#include "CommandFactory.h"

#include "string/DelCommand.h"
#include "string/ExistsCommand.h"
#include "string/GetCommand.h"
#include "string/SetCommand.h"
#include "system/InfoCommand.h"
#include "system/PingCommand.h"

namespace swiftcache {

CommandRegistry buildCommandRegistry(std::chrono::steady_clock::time_point startedAt,
                                     const ServerMetrics& metrics) {
    CommandRegistry registry;
    registry.registerCommand(std::make_unique<PingCommand>());
    registry.registerCommand(std::make_unique<SetCommand>());
    registry.registerCommand(std::make_unique<GetCommand>());
    registry.registerCommand(std::make_unique<DelCommand>());
    registry.registerCommand(std::make_unique<ExistsCommand>());
    registry.registerCommand(std::make_unique<InfoCommand>(startedAt, metrics));
    return registry;
}

} // namespace swiftcache
