#include "AofPersistence.h"

#include <algorithm>
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
    std::ifstream input(path_, std::ios::binary);
    if (!input.is_open()) {
        return true;
    }

    std::string buffer((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    CommandParser parser;
    const auto commands = parser.parseAvailable(buffer);

    for (const auto& command : commands) {
        if (command.tokens.empty()) {
            continue;
        }
        const auto result = registry.execute(command.tokens, store);
        if (result.response.rfind("ERR ", 0) == 0) {
            std::cerr << "AOF replay skipped failed command: " << result.response;
        }
    }

    if (!buffer.empty()) {
        std::cerr << "AOF replay ignored trailing incomplete data in " << path_ << "\n";
    }

    return true;
}

bool AofPersistence::append(const std::vector<std::string>& tokens) {
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

    output << encodeRespCommand(tokens);
    output.flush();
    return output.good();
}

CommandResult AofPersistence::executeAndAppend(const std::vector<std::string>& tokens,
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

    output << encodeRespCommand(tokens);
    output.flush();
    appendSucceeded = output.good();
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
    return output.good();
}

bool AofPersistence::isMutatingCommand(const std::vector<std::string>& tokens) const {
    if (tokens.empty()) {
        return false;
    }
    return kMutatingCommands.find(toUpper(tokens.front())) != kMutatingCommands.end();
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

} // namespace swiftcache
