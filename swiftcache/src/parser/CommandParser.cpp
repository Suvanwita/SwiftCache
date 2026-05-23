#include "CommandParser.h"

#include <sstream>

namespace swiftcache {

std::vector<std::string> CommandParser::parse(const std::string& line) const {
    std::istringstream stream(line);
    std::vector<std::string> tokens;
    std::string token;

    while (stream >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

} // namespace swiftcache
