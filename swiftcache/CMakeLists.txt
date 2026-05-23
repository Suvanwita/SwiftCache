cmake_minimum_required(VERSION 3.10)

project(SwiftCache VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

set(SWIFTCACHE_SOURCES
    src/core/CommandRegistry.cpp
    src/core/ExpiryWorker.cpp
    src/core/ServerMetrics.cpp
    src/datastore/DataStore.cpp
    src/parser/CommandParser.cpp
    src/utils/StringUtils.cpp
    src/networking/TcpServer.cpp
    src/commands/CommandFactory.cpp
    src/commands/hash/HdelCommand.cpp
    src/commands/hash/HexistsCommand.cpp
    src/commands/hash/HgetCommand.cpp
    src/commands/hash/HgetallCommand.cpp
    src/commands/hash/HsetCommand.cpp
    src/commands/list/LpopCommand.cpp
    src/commands/list/LpushCommand.cpp
    src/commands/list/LrangeCommand.cpp
    src/commands/list/RpopCommand.cpp
    src/commands/list/RpushCommand.cpp
    src/commands/set/SaddCommand.cpp
    src/commands/set/ScardCommand.cpp
    src/commands/set/SismemberCommand.cpp
    src/commands/set/SmembersCommand.cpp
    src/commands/set/SremCommand.cpp
    src/commands/string/AppendCommand.cpp
    src/commands/string/DecrCommand.cpp
    src/commands/string/SetCommand.cpp
    src/commands/string/GetCommand.cpp
    src/commands/string/DelCommand.cpp
    src/commands/string/ExpireCommand.cpp
    src/commands/string/ExistsCommand.cpp
    src/commands/string/IncrCommand.cpp
    src/commands/string/MgetCommand.cpp
    src/commands/string/MsetCommand.cpp
    src/commands/string/PersistCommand.cpp
    src/commands/string/StrlenCommand.cpp
    src/commands/string/TtlCommand.cpp
    src/commands/system/PingCommand.cpp
    src/commands/system/InfoCommand.cpp
)

add_library(swiftcache_core ${SWIFTCACHE_SOURCES})
target_include_directories(swiftcache_core PUBLIC src)
target_link_libraries(swiftcache_core PUBLIC Threads::Threads)

add_executable(SwiftCache src/main.cpp)
target_link_libraries(SwiftCache PRIVATE swiftcache_core)

add_executable(swiftcache_tests tests/CommandTests.cpp)
target_link_libraries(swiftcache_tests PRIVATE swiftcache_core)

enable_testing()
add_test(NAME swiftcache_command_tests COMMAND swiftcache_tests)
