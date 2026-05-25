#include <algorithm>
#include <chrono>
#include <csignal>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "commands/CommandFactory.h"
#include "core/AofPersistence.h"
#include "core/ExpiryWorker.h"
#include "core/SnapshotPersistence.h"
#include "core/SnapshotWorker.h"
#include "datastore/DataStore.h"
#include "networking/TcpServer.h"

namespace {

volatile std::sig_atomic_t g_shutdownRequested = 0;

#ifndef SWIFTCACHE_AOF_PATH
#define SWIFTCACHE_AOF_PATH "storage/swiftcache.aof"
#endif

#ifndef SWIFTCACHE_SNAPSHOT_PATH
#define SWIFTCACHE_SNAPSHOT_PATH "storage/swiftcache.snapshot"
#endif

void handleSignal(int) {
    g_shutdownRequested = 1;
}

struct ServerConfig {
    std::string host{"localhost"};
    std::uint16_t port{6379};
    std::string aofPath{SWIFTCACHE_AOF_PATH};
    std::string snapshotPath{SWIFTCACHE_SNAPSHOT_PATH};
    bool aofEnabled{true};
    bool snapshotEnabled{true};
    swiftcache::EvictionConfig eviction;
};

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base();

    if (first >= last) {
        return "";
    }
    return std::string(first, last);
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool parseBool(const std::string& raw, bool& parsed) {
    const auto value = toLower(trim(raw));
    if (value == "1" || value == "true" || value == "yes" || value == "on") {
        parsed = true;
        return true;
    }
    if (value == "0" || value == "false" || value == "no" || value == "off") {
        parsed = false;
        return true;
    }
    return false;
}

bool parsePort(const std::string& raw, std::uint16_t& port) {
    try {
        std::size_t consumed = 0;
        const auto parsed = std::stoul(raw, &consumed);
        if (consumed != raw.size() || parsed == 0 || parsed > 65535) {
            return false;
        }
        port = static_cast<std::uint16_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseSize(const std::string& raw, std::size_t& size) {
    try {
        std::size_t consumed = 0;
        const auto parsed = std::stoull(raw, &consumed);
        if (consumed != raw.size()) {
            return false;
        }
        size = static_cast<std::size_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseEvictionPolicy(const std::string& raw, swiftcache::EvictionPolicy& policy) {
    const auto value = toLower(trim(raw));
    if (value == "none" || value == "noeviction") {
        policy = swiftcache::EvictionPolicy::None;
        return true;
    }
    if (value == "allkeys-lru") {
        policy = swiftcache::EvictionPolicy::AllKeysLru;
        return true;
    }
    if (value == "volatile-lru") {
        policy = swiftcache::EvictionPolicy::VolatileLru;
        return true;
    }
    if (value == "ttl-priority") {
        policy = swiftcache::EvictionPolicy::TtlPriority;
        return true;
    }
    if (value == "random") {
        policy = swiftcache::EvictionPolicy::Random;
        return true;
    }
    return false;
}

bool applyConfigOption(ServerConfig& config, const std::string& key, const std::string& value,
                       std::string& error) {
    const auto normalizedKey = toLower(trim(key));
    const auto normalizedValue = trim(value);

    if (normalizedKey == "host") {
        config.host = normalizedValue;
        return true;
    }
    if (normalizedKey == "port") {
        if (!parsePort(normalizedValue, config.port)) {
            error = "invalid port: " + normalizedValue;
            return false;
        }
        return true;
    }
    if (normalizedKey == "aof") {
        config.aofPath = normalizedValue;
        return true;
    }
    if (normalizedKey == "snapshot") {
        config.snapshotPath = normalizedValue;
        return true;
    }
    if (normalizedKey == "aof_enabled" || normalizedKey == "appendonly") {
        if (!parseBool(normalizedValue, config.aofEnabled)) {
            error = "invalid boolean for " + normalizedKey + ": " + normalizedValue;
            return false;
        }
        return true;
    }
    if (normalizedKey == "snapshot_enabled") {
        if (!parseBool(normalizedValue, config.snapshotEnabled)) {
            error = "invalid boolean for snapshot_enabled: " + normalizedValue;
            return false;
        }
        return true;
    }
    if (normalizedKey == "max_keys") {
        if (!parseSize(normalizedValue, config.eviction.maxKeys)) {
            error = "invalid max_keys: " + normalizedValue;
            return false;
        }
        return true;
    }
    if (normalizedKey == "max_memory") {
        if (!parseSize(normalizedValue, config.eviction.maxMemoryBytes)) {
            error = "invalid max_memory: " + normalizedValue;
            return false;
        }
        return true;
    }
    if (normalizedKey == "eviction_policy") {
        if (!parseEvictionPolicy(normalizedValue, config.eviction.policy)) {
            error = "invalid eviction_policy: " + normalizedValue;
            return false;
        }
        return true;
    }

    error = "unknown config option: " + normalizedKey;
    return false;
}

bool loadConfigFile(const std::string& path, ServerConfig& config) {
    std::ifstream input(path);
    if (!input.is_open()) {
        std::cerr << "SwiftCache failed to open config file: " << path << "\n";
        return false;
    }

    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const auto comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }

        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            std::cerr << path << ":" << lineNumber << ": expected key=value\n";
            return false;
        }

        std::string error;
        if (!applyConfigOption(config, line.substr(0, separator), line.substr(separator + 1), error)) {
            std::cerr << path << ":" << lineNumber << ": " << error << "\n";
            return false;
        }
    }

    return true;
}

std::string optionValue(const std::string& arg, const std::string& prefix) {
    if (arg.rfind(prefix + "=", 0) == 0) {
        return arg.substr(prefix.size() + 1);
    }
    return "";
}

std::string findConfigPath(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto inlineValue = optionValue(arg, "--config");
        if (!inlineValue.empty()) {
            return inlineValue;
        }
        if (arg == "--config" && i + 1 < argc) {
            return argv[i + 1];
        }
    }
    return "";
}

void printUsage() {
    std::cout
        << "Usage: SwiftCache [options]\n"
        << "Options:\n"
        << "  --host <host>          Bind host. Defaults to localhost.\n"
        << "  --port <port>          Bind port. Defaults to 6379.\n"
        << "  --aof <path>           Append-only file path.\n"
        << "  --snapshot <path>      Snapshot file path.\n"
        << "  --config <path>        Load key=value config before CLI overrides.\n"
        << "  --max-keys <count>     Evict keys above this key count. 0 disables.\n"
        << "  --max-memory <bytes>   Evict keys above this estimated memory. 0 disables.\n"
        << "  --eviction-policy <p>  none, allkeys-lru, volatile-lru, ttl-priority, random.\n"
        << "  --no-aof              Disable AOF replay and writes.\n"
        << "  --no-snapshot         Disable snapshot load and background saves.\n"
        << "  --help                Show this help.\n";
}

bool parseCommandLine(int argc, char* argv[], ServerConfig& config, bool& shouldExit) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto readValue = [&](const std::string& name, std::string& value) -> bool {
            const auto inlineValue = optionValue(arg, name);
            if (!inlineValue.empty()) {
                value = inlineValue;
                return true;
            }
            if (arg == name && i + 1 < argc) {
                value = argv[++i];
                return true;
            }
            return false;
        };

        std::string value;
        if (readValue("--host", value)) {
            config.host = value;
        } else if (readValue("--port", value)) {
            if (!parsePort(value, config.port)) {
                std::cerr << "invalid --port value: " << value << "\n";
                return false;
            }
        } else if (readValue("--aof", value)) {
            config.aofPath = value;
        } else if (readValue("--snapshot", value)) {
            config.snapshotPath = value;
        } else if (readValue("--config", value)) {
            continue;
        } else if (readValue("--max-keys", value)) {
            if (!parseSize(value, config.eviction.maxKeys)) {
                std::cerr << "invalid --max-keys value: " << value << "\n";
                return false;
            }
        } else if (readValue("--max-memory", value)) {
            if (!parseSize(value, config.eviction.maxMemoryBytes)) {
                std::cerr << "invalid --max-memory value: " << value << "\n";
                return false;
            }
        } else if (readValue("--eviction-policy", value)) {
            if (!parseEvictionPolicy(value, config.eviction.policy)) {
                std::cerr << "invalid --eviction-policy value: " << value << "\n";
                return false;
            }
        } else if (arg == "--no-aof") {
            config.aofEnabled = false;
        } else if (arg == "--no-snapshot") {
            config.snapshotEnabled = false;
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            shouldExit = true;
            return true;
        } else {
            std::cerr << "unknown option: " << arg << "\n";
            printUsage();
            return false;
        }
    }

    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    ServerConfig config;
    const auto configPath = findConfigPath(argc, argv);
    if (!configPath.empty() && !loadConfigFile(configPath, config)) {
        return 1;
    }

    bool shouldExit = false;
    if (!parseCommandLine(argc, argv, config, shouldExit)) {
        return 1;
    }
    if (shouldExit) {
        return 0;
    }

    swiftcache::DataStore store;
    store.configureEviction(config.eviction);
    swiftcache::ServerMetrics metrics;
    swiftcache::ExpiryWorker expiryWorker(store);

    std::unique_ptr<swiftcache::AofPersistence> aof;
    if (config.aofEnabled) {
        aof = std::make_unique<swiftcache::AofPersistence>(config.aofPath);
    }

    auto registry = swiftcache::buildCommandRegistry(std::chrono::steady_clock::now(), metrics,
                                                     aof.get());

    std::unique_ptr<swiftcache::SnapshotPersistence> snapshot;
    if (config.snapshotEnabled) {
        snapshot = std::make_unique<swiftcache::SnapshotPersistence>(config.snapshotPath);
        if (!snapshot->load(store)) {
            std::cerr << "SwiftCache failed to load snapshot\n";
            return 1;
        }
    }

    if (aof != nullptr && !aof->replay(registry, store)) {
        std::cerr << "SwiftCache failed to replay AOF\n";
        return 1;
    }

    swiftcache::TcpServer server(config.host, config.port, store, registry, metrics, aof.get());
    std::unique_ptr<swiftcache::SnapshotWorker> snapshotWorker;
    if (snapshot != nullptr) {
        snapshotWorker = std::make_unique<swiftcache::SnapshotWorker>(store, *snapshot, aof.get(),
                                                                      &metrics);
    }

    expiryWorker.start();
    if (snapshotWorker != nullptr) {
        snapshotWorker->start();
    }

    if (g_shutdownRequested) {
        server.stop();
        if (snapshotWorker != nullptr) {
            snapshotWorker->stop();
        }
        expiryWorker.stop();
        return 0;
    }

    const bool started = server.start();
    if (snapshotWorker != nullptr) {
        snapshotWorker->stop();
    }
    expiryWorker.stop();
    if (!started) {
        std::cerr << "SwiftCache failed to start\n";
        return 1;
    }

    return 0;
}
