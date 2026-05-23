#include "InfoCommand.h"

#include <chrono>
#include <sstream>

namespace swiftcache {

InfoCommand::InfoCommand(std::chrono::steady_clock::time_point startedAt, const ServerMetrics& metrics)
    : startedAt_(startedAt), metrics_(metrics) {}

std::string InfoCommand::name() const {
    return "INFO";
}

CommandResult InfoCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (!args.empty()) {
        return {"ERR wrong number of arguments for INFO\n", false};
    }

    const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - startedAt_);
    const auto stats = store.stats();

    std::ostringstream out;
    out << "{\n";
    out << " totalKeys: " << stats.keys << ",\n";
    out << " connectedClients: " << metrics_.connectedClients() << ",\n";
    out << " totalCommands: " << metrics_.totalCommands() << ",\n";
    out << " uptimeSeconds: " << uptime.count() << "\n";
    out << "}\n";
    return {out.str(), false};
}

} // namespace swiftcache
