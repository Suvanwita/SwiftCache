#include "AofPersistence.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>

#include "../parser/CommandParser.h"
#include "../utils/StringUtils.h"

namespace swiftcache {
namespace {

const std::unordered_set<std::string> kMutatingCommands{
    "SET", "MSET", "DEL", "EXPIRE", "PERSIST",
    "INCR", "DECR", "APPEND",
    "LPUSH", "RPUSH", "LPOP", "RPOP",
    "HSET", "HDEL",
    "SADD", "SREM",
    "RENAME", "FLUSHDB"
};

} // namespace

AofPersistence::AofPersistence(std::string path) : path_(std::move(path)) {}

bool AofPersistence::replay(const CommandRegistry& registry, DataStore& store) const {
    std::deque<DataStore> stores(1);
    const bool replayed = replay(registry, stores);
    if (!replayed) {
        return false;
    }
    store.loadSnapshot(stores[0].snapshot());
    return true;
}

bool AofPersistence::replay(const CommandRegistry& registry, std::deque<DataStore>& stores) const {
    std::ifstream input(path_, std::ios::binary);
    if (!input.is_open()) {
        return true;
    }

    std::string buffer((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    CommandParser parser;
    const auto commands = parser.parseAvailable(buffer);
    std::size_t databaseIndex = 0;

    for (const auto& command : commands) {
        if (command.tokens.empty()) {
            continue;
        }

        const auto commandName = toUpper(command.tokens.front());
        if (commandName == "SELECT") {
            if (command.tokens.size() != 2) {
                std::cerr << "AOF replay skipped invalid SELECT command\n";
                continue;
            }

            try {
                std::size_t consumed = 0;
                const auto parsed = std::stoull(command.tokens[1], &consumed);
                if (consumed != command.tokens[1].size() || parsed >= stores.size()) {
                    std::cerr << "AOF replay skipped out-of-range SELECT DB: "
                              << command.tokens[1] << "\n";
                    continue;
                }
                databaseIndex = static_cast<std::size_t>(parsed);
            } catch (const std::exception&) {
                std::cerr << "AOF replay skipped invalid SELECT DB: " << command.tokens[1] << "\n";
            }
            continue;
        }

        const auto result = registry.execute(command.tokens, stores[databaseIndex]);
        if (result.response.rfind("ERR ", 0) == 0) {
            std::cerr << "AOF replay skipped failed command: " << result.response;
        }
    }

    if (!buffer.empty()) {
        std::cerr << "AOF replay ignored trailing incomplete data in " << path_ << "\n";
    }

    return true;
}

bool AofPersistence::append(const std::vector<std::string>& tokens, std::size_t databaseIndex) {
    if (!isMutatingCommand(tokens)) {
        return true;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const auto parent = std::filesystem::path(path_).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream output(path_, std::ios::binary | std::ios::app);
    if (!output.is_open()) {
        return false;
    }

    if (!writeCommand(output, tokens, databaseIndex)) {
        return false;
    }
    output.flush();
    return output.good();
}

CommandResult AofPersistence::executeAndAppend(const std::vector<std::string>& tokens,
                                               std::size_t databaseIndex,
                                               const std::function<CommandResult()>& execute,
                                               bool& appendSucceeded) {
    appendSucceeded = true;
    if (!isMutatingCommand(tokens)) {
        return execute();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const auto result = execute();
    if (result.response.rfind("ERR ", 0) == 0) {
        return result;
    }

    const auto parent = std::filesystem::path(path_).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream output(path_, std::ios::binary | std::ios::app);
    if (!output.is_open()) {
        appendSucceeded = false;
        return result;
    }

    appendSucceeded = writeCommand(output, tokens, databaseIndex);
    output.flush();
    appendSucceeded = appendSucceeded && output.good();
    return result;
}

bool AofPersistence::checkpoint(const std::function<bool()>& saveSnapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!saveSnapshot()) {
        return false;
    }

    const auto parent = std::filesystem::path(path_).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream output(path_, std::ios::binary | std::ios::trunc);
    currentAppendDatabase_ = 0;
    return output.good();
}

bool AofPersistence::isMutatingCommand(const std::vector<std::string>& tokens) const {
    if (tokens.empty()) {
        return false;
    }
    return kMutatingCommands.find(toUpper(tokens.front())) != kMutatingCommands.end();
}

std::uintmax_t AofPersistence::sizeBytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::error_code error;
    const auto size = std::filesystem::file_size(path_, error);
    return error ? 0 : size;
}

const std::string& AofPersistence::path() const {
    return path_;
}

std::string AofPersistence::encodeRespCommand(const std::vector<std::string>& tokens) {
    std::string encoded = "*" + std::to_string(tokens.size()) + "\r\n";
    for (const auto& token : tokens) {
        encoded += "$" + std::to_string(token.size()) + "\r\n";
        encoded += token;
        encoded += "\r\n";
    }
    return encoded;
}

bool AofPersistence::writeCommand(std::ofstream& output, const std::vector<std::string>& tokens,
                                  std::size_t databaseIndex) {
    if (databaseIndex != currentAppendDatabase_) {
        output << encodeRespCommand({"SELECT", std::to_string(databaseIndex)});
        currentAppendDatabase_ = databaseIndex;
    }
    output << encodeRespCommand(tokens);
    return output.good();
}

} // namespace swiftcache
