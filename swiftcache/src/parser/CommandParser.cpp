#include "CommandParser.h"

#include <cstdlib>
#include <sstream>

namespace swiftcache {
namespace {

bool parseIntegerLine(const std::string& buffer, std::size_t& offset, long long& value) {
    const std::size_t lineEnd = buffer.find("\r\n", offset);
    if (lineEnd == std::string::npos) {
        return false;
    }

    const std::string raw = buffer.substr(offset, lineEnd - offset);
    char* end = nullptr;
    value = std::strtoll(raw.c_str(), &end, 10);
    if (end == raw.c_str() || *end != '\0') {
        value = -1;
    }
    offset = lineEnd + 2;
    return true;
}

} // namespace

std::vector<std::string> CommandParser::parse(const std::string& line) const {
    std::istringstream stream(line);
    std::vector<std::string> tokens;
    std::string token;

    while (stream >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

std::vector<ParsedCommand> CommandParser::parseAvailable(std::string& buffer) const {
    std::vector<ParsedCommand> commands;

    while (!buffer.empty()) {
        if (buffer.front() == '*') {
            std::size_t consumed = 0;
            std::vector<std::string> tokens;
            if (!parseRespFrame(buffer, consumed, tokens)) {
                break;
            }

            commands.push_back(ParsedCommand{tokens, RequestProtocol::Resp});
            buffer.erase(0, consumed);
            continue;
        }

        const std::size_t newline = buffer.find('\n');
        if (newline == std::string::npos) {
            break;
        }

        std::string line = buffer.substr(0, newline + 1);
        buffer.erase(0, newline + 1);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }

        if (!line.empty()) {
            commands.push_back(ParsedCommand{parse(line), RequestProtocol::Inline});
        }
    }

    return commands;
}

bool CommandParser::parseRespFrame(const std::string& buffer, std::size_t& consumed,
                                   std::vector<std::string>& tokens) {
    std::size_t offset = 1;
    long long elementCount = 0;
    if (!parseIntegerLine(buffer, offset, elementCount)) {
        return false;
    }
    if (elementCount < 0) {
        consumed = offset;
        return true;
    }

    std::vector<std::string> parsed;
    parsed.reserve(static_cast<std::size_t>(elementCount));

    for (long long i = 0; i < elementCount; ++i) {
        if (offset >= buffer.size()) {
            return false;
        }
        if (buffer[offset] != '$') {
            consumed = offset + 1;
            tokens.clear();
            return true;
        }

        ++offset;
        long long bulkLength = 0;
        if (!parseIntegerLine(buffer, offset, bulkLength)) {
            return false;
        }
        if (bulkLength < 0) {
            parsed.emplace_back();
            continue;
        }

        const auto length = static_cast<std::size_t>(bulkLength);
        if (buffer.size() < offset + length + 2) {
            return false;
        }
        if (buffer[offset + length] != '\r' || buffer[offset + length + 1] != '\n') {
            consumed = offset + length;
            tokens.clear();
            return true;
        }

        parsed.push_back(buffer.substr(offset, length));
        offset += length + 2;
    }

    consumed = offset;
    tokens = std::move(parsed);
    return true;
}

} // namespace swiftcache
