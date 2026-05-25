#include "InfoCommand.h"

#include <chrono>
#include <cstddef>
#include <sstream>

namespace swiftcache {

InfoCommand::InfoCommand(std::chrono::steady_clock::time_point startedAt,
                         const ServerMetrics& metrics, const AofPersistence* aof)
    : startedAt_(startedAt), metrics_(metrics), aof_(aof) {}

std::string InfoCommand::name() const {
    return "INFO";
}

CommandResult InfoCommand::execute(const std::vector<std::string>& args, DataStore& store) {
    if (!args.empty()) {
        return {"ERR wrong number of arguments for INFO\n", false};
    }

    const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - startedAt_);
    store.removeExpired();
    const auto stats = store.stats();

    std::ostringstream out;
    out << "{\n";
    out << " totalKeys: " << stats.keys << ",\n";
    out << " estimatedBytes: " << stats.estimatedBytes << ",\n";
    out << " aofSizeBytes: " << (aof_ == nullptr ? 0 : aof_->sizeBytes()) << ",\n";
    out << " evictedKeys: " << stats.evictedKeys << ",\n";
    out << " expiredKeys: " << stats.expiredKeys << ",\n";
    out << " connectedClients: " << metrics_.connectedClients() << ",\n";
    out << " peakConnectedClients: " << metrics_.peakConnectedClients() << ",\n";
    out << " totalCommands: " << metrics_.totalCommands() << ",\n";
    out << " rejectedCommands: " << metrics_.rejectedCommands() << ",\n";
    out << " lastSnapshotUnixSeconds: " << metrics_.lastSnapshotUnixSeconds() << ",\n";
    out << " commandCounts: {\n";
    const auto commandCounts = metrics_.commandCounts();
    for (std::size_t i = 0; i < commandCounts.size(); ++i) {
        out << "  " << commandCounts[i].first << ": " << commandCounts[i].second;
        if (i + 1 < commandCounts.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << " },\n";
    out << " uptimeSeconds: " << uptime.count() << "\n";
    out << "}\n";
    return {out.str(), false};
}

} // namespace swiftcache
