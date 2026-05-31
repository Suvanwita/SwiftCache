#include "CommandMetadata.h"

#include <unordered_map>

#include "../utils/StringUtils.h"

namespace swiftcache {
namespace {

const std::unordered_map<std::string, CommandMetadata>& metadataByCommand() {
    static const std::unordered_map<std::string, CommandMetadata> metadata{
        {"GET", {CommandCategory::Read, false, false, true}},
        {"EXISTS", {CommandCategory::Read, false, false, true}},
        {"TTL", {CommandCategory::Read, false, false, true}},
        {"STRLEN", {CommandCategory::Read, false, false, true}},
        {"MGET", {CommandCategory::Read, false, false, true}},
        {"LRANGE", {CommandCategory::Read, false, false, true}},
        {"HGET", {CommandCategory::Read, false, false, true}},
        {"HEXISTS", {CommandCategory::Read, false, false, true}},
        {"HGETALL", {CommandCategory::Read, false, false, true}},
        {"SISMEMBER", {CommandCategory::Read, false, false, true}},
        {"SMEMBERS", {CommandCategory::Read, false, false, true}},
        {"SCARD", {CommandCategory::Read, false, false, true}},
        {"KEYS", {CommandCategory::Read, false, false, true}},
        {"DBSIZE", {CommandCategory::Read, false, false, true}},
        {"MEMORY", {CommandCategory::Read, false, false, true}},
        {"TYPE", {CommandCategory::Read, false, false, true}},
        {"SCAN", {CommandCategory::Read, false, false, true}},

        {"SET", {CommandCategory::Write, true, true, false}},
        {"MSET", {CommandCategory::Write, true, true, false}},
        {"DEL", {CommandCategory::Write, true, true, false}},
        {"EXPIRE", {CommandCategory::Write, true, true, false}},
        {"PERSIST", {CommandCategory::Write, true, true, false}},
        {"INCR", {CommandCategory::Write, true, true, false}},
        {"DECR", {CommandCategory::Write, true, true, false}},
        {"APPEND", {CommandCategory::Write, true, true, false}},
        {"LPUSH", {CommandCategory::Write, true, true, false}},
        {"RPUSH", {CommandCategory::Write, true, true, false}},
        {"LPOP", {CommandCategory::Write, true, true, false}},
        {"RPOP", {CommandCategory::Write, true, true, false}},
        {"HSET", {CommandCategory::Write, true, true, false}},
        {"HDEL", {CommandCategory::Write, true, true, false}},
        {"SADD", {CommandCategory::Write, true, true, false}},
        {"SREM", {CommandCategory::Write, true, true, false}},
        {"RENAME", {CommandCategory::Write, true, true, false}},
        {"MOVE", {CommandCategory::Write, true, true, false}},
        {"FLUSHDB", {CommandCategory::Write, true, true, false}},
        {"FLUSHALL", {CommandCategory::Write, true, true, false}},

        {"PING", {CommandCategory::Admin, false, false, true}},
        {"INFO", {CommandCategory::Admin, false, false, true}},
        {"AUTH", {CommandCategory::Admin, false, false, true}},
        {"READONLY", {CommandCategory::Admin, false, false, true}},
        {"CONFIG", {CommandCategory::Admin, false, false, true}},
        {"COMMAND", {CommandCategory::Admin, false, false, true}},
        {"SELECT", {CommandCategory::Admin, false, false, true}},

        {"SAVE", {CommandCategory::Persistence, false, false, true}},
        {"LASTSAVE", {CommandCategory::Persistence, false, false, true}},

        {"SUBSCRIBE", {CommandCategory::PubSub, false, false, true}},
        {"UNSUBSCRIBE", {CommandCategory::PubSub, false, false, true}},
        {"PUBLISH", {CommandCategory::PubSub, false, false, false}},

        {"CLIENT", {CommandCategory::Client, false, false, true}},
    };
    return metadata;
}

} // namespace

CommandMetadata commandMetadata(const std::string& commandName) {
    const auto it = metadataByCommand().find(toUpper(commandName));
    if (it == metadataByCommand().end()) {
        return {};
    }
    return it->second;
}

CommandMetadata commandMetadata(const std::vector<std::string>& tokens) {
    if (tokens.empty()) {
        return {};
    }
    return commandMetadata(tokens.front());
}

bool isAofLoggedCommand(const std::vector<std::string>& tokens) {
    return commandMetadata(tokens).appendToAof;
}

bool isAllowedInReadOnly(const std::vector<std::string>& tokens) {
    return commandMetadata(tokens).allowedInReadOnly;
}

} // namespace swiftcache
