#include "CommandFactory.h"

#include "hash/HdelCommand.h"
#include "hash/HexistsCommand.h"
#include "hash/HgetCommand.h"
#include "hash/HgetallCommand.h"
#include "hash/HsetCommand.h"
#include "list/LpopCommand.h"
#include "list/LpushCommand.h"
#include "list/LrangeCommand.h"
#include "list/RpopCommand.h"
#include "list/RpushCommand.h"
#include "string/AppendCommand.h"
#include "string/DecrCommand.h"
#include "string/DelCommand.h"
#include "string/ExpireCommand.h"
#include "string/ExistsCommand.h"
#include "string/GetCommand.h"
#include "string/IncrCommand.h"
#include "string/MgetCommand.h"
#include "string/MsetCommand.h"
#include "string/PersistCommand.h"
#include "string/SetCommand.h"
#include "string/StrlenCommand.h"
#include "string/TtlCommand.h"
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
    registry.registerCommand(std::make_unique<ExpireCommand>());
    registry.registerCommand(std::make_unique<TtlCommand>());
    registry.registerCommand(std::make_unique<PersistCommand>());
    registry.registerCommand(std::make_unique<IncrCommand>());
    registry.registerCommand(std::make_unique<DecrCommand>());
    registry.registerCommand(std::make_unique<AppendCommand>());
    registry.registerCommand(std::make_unique<StrlenCommand>());
    registry.registerCommand(std::make_unique<MgetCommand>());
    registry.registerCommand(std::make_unique<MsetCommand>());
    registry.registerCommand(std::make_unique<LpushCommand>());
    registry.registerCommand(std::make_unique<RpushCommand>());
    registry.registerCommand(std::make_unique<LpopCommand>());
    registry.registerCommand(std::make_unique<RpopCommand>());
    registry.registerCommand(std::make_unique<LrangeCommand>());
    registry.registerCommand(std::make_unique<HsetCommand>());
    registry.registerCommand(std::make_unique<HgetCommand>());
    registry.registerCommand(std::make_unique<HdelCommand>());
    registry.registerCommand(std::make_unique<HexistsCommand>());
    registry.registerCommand(std::make_unique<HgetallCommand>());
    registry.registerCommand(std::make_unique<InfoCommand>(startedAt, metrics));
    return registry;
}

} // namespace swiftcache
