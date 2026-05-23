#pragma once

#include <string>
#include <vector>

namespace swiftcache {

enum class RequestProtocol {
    Inline,
    Resp
};

struct ParsedCommand {
    std::vector<std::string> tokens;
    RequestProtocol protocol{RequestProtocol::Inline};
};

class CommandParser {
public:
    std::vector<std::string> parse(const std::string& line) const;
    std::vector<ParsedCommand> parseAvailable(std::string& buffer) const;

private:
    static bool parseRespFrame(const std::string& buffer, std::size_t& consumed,
                               std::vector<std::string>& tokens);
};

} // namespace swiftcache
