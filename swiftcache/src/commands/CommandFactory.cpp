#include "CommandFactory.h"

#include "hash/HdelCommand.h"
#include "hash/HexistsCommand.h"
#include "hash/HgetCommand.h"
#include "hash/HgetallCommand.h"
#include "hash/HsetCommand.h"
#include "key/DbsizeCommand.h"
#include "key/FlushdbCommand.h"
#include "key/KeysCommand.h"
#include "key/RenameCommand.h"
#include "key/ScanCommand.h"
#include "key/TypeCommand.h"
#include "list/LpopCommand.h"
#include "list/LpushCommand.h"
#include "list/LrangeCommand.h"
#include "list/RpopCommand.h"
#include "list/RpushCommand.h"
#include "set/SaddCommand.h"
#include "set/ScardCommand.h"
#include "set/SismemberCommand.h"
#include "set/SmembersCommand.h"
#include "set/SremCommand.h"
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
                                     const ServerMetrics& metrics, const AofPersistence* aof) {
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
    registry.registerCommand(std::make_unique<SaddCommand>());
    registry.registerCommand(std::make_unique<SremCommand>());
    registry.registerCommand(std::make_unique<SismemberCommand>());
    registry.registerCommand(std::make_unique<SmembersCommand>());
    registry.registerCommand(std::make_unique<ScardCommand>());
    registry.registerCommand(std::make_unique<KeysCommand>());
    registry.registerCommand(std::make_unique<DbsizeCommand>());
    registry.registerCommand(std::make_unique<TypeCommand>());
    registry.registerCommand(std::make_unique<RenameCommand>());
    registry.registerCommand(std::make_unique<FlushdbCommand>());
    registry.registerCommand(std::make_unique<ScanCommand>());
    registry.registerCommand(std::make_unique<InfoCommand>(startedAt, metrics, aof));
    return registry;
}

} // namespace swiftcache
