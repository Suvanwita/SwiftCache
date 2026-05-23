#pragma once

#include <chrono>

#include "../core/CommandRegistry.h"
#include "../core/ServerMetrics.h"

namespace swiftcache {

CommandRegistry buildCommandRegistry(std::chrono::steady_clock::time_point startedAt,
                                     const ServerMetrics& metrics);

} // namespace swiftcache
