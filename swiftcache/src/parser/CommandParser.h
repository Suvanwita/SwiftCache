#pragma once

#include <string>
#include <vector>

namespace swiftcache {

class CommandParser {
public:
    std::vector<std::string> parse(const std::string& line) const;
};

} // namespace swiftcache
