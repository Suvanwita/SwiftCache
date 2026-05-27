#include "SnapshotPersistence.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace swiftcache {
namespace {

constexpr const char* kMagic = "SWIFTCACHE_SNAPSHOT_V1";
constexpr const char* kMultiDbMagic = "SWIFTCACHE_SNAPSHOT_V2";

void writeLine(std::ostream& output, const std::string& value) {
    output << value << "\n";
}

bool readLine(std::istream& input, std::string& value) {
    return static_cast<bool>(std::getline(input, value));
}

void writeBulk(std::ostream& output, const std::string& value) {
    output << value.size() << "\n";
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
    output << "\n";
}

bool readBulk(std::istream& input, std::string& value) {
    std::string sizeLine;
    if (!readLine(input, sizeLine)) {
        return false;
    }

    std::size_t size = 0;
    try {
        size = static_cast<std::size_t>(std::stoull(sizeLine));
    } catch (const std::exception&) {
        return false;
    }

    value.assign(size, '\0');
    input.read(value.data(), static_cast<std::streamsize>(size));
    if (input.gcount() != static_cast<std::streamsize>(size)) {
        return false;
    }

    char newline = '\0';
    input.get(newline);
    return newline == '\n';
}

bool readLongLong(std::istream& input, long long& value) {
    std::string line;
    if (!readLine(input, line)) {
        return false;
    }

    try {
        value = std::stoll(line);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool readSize(std::istream& input, std::size_t& value) {
    std::string line;
    if (!readLine(input, line)) {
        return false;
    }

    try {
        value = static_cast<std::size_t>(std::stoull(line));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool writeEntries(std::ostream& output, const std::vector<SnapshotEntry>& entries) {
    writeLine(output, std::to_string(entries.size()));

    for (const auto& entry : entries) {
        writeBulk(output, entry.key);
        writeBulk(output, entry.value.type);
        writeLine(output, std::to_string(entry.value.createdAt));
        writeLine(output, std::to_string(entry.value.expiresAt));

        if (entry.value.type == data_type::String) {
            writeBulk(output, entry.value.value);
        } else if (entry.value.type == data_type::List) {
            writeLine(output, std::to_string(entry.value.list.size()));
            for (const auto& value : entry.value.list) {
                writeBulk(output, value);
            }
        } else if (entry.value.type == data_type::Hash) {
            std::vector<std::pair<std::string, std::string>> fields(entry.value.hash.begin(),
                                                                    entry.value.hash.end());
            std::sort(fields.begin(), fields.end(), [](const auto& lhs, const auto& rhs) {
                return lhs.first < rhs.first;
            });
            writeLine(output, std::to_string(fields.size()));
            for (const auto& field : fields) {
                writeBulk(output, field.first);
                writeBulk(output, field.second);
            }
        } else if (entry.value.type == data_type::Set) {
            std::vector<std::string> members(entry.value.set.begin(), entry.value.set.end());
            std::sort(members.begin(), members.end());
            writeLine(output, std::to_string(members.size()));
            for (const auto& member : members) {
                writeBulk(output, member);
            }
        } else {
            return false;
        }
    }

    return output.good();
}

bool readEntries(std::istream& input, std::vector<SnapshotEntry>& entries) {
    std::size_t entryCount = 0;
    if (!readSize(input, entryCount)) {
        return false;
    }

    entries.clear();
    entries.reserve(entryCount);

    for (std::size_t i = 0; i < entryCount; ++i) {
        SnapshotEntry entry;
        if (!readBulk(input, entry.key) || !readBulk(input, entry.value.type) ||
            !readLongLong(input, entry.value.createdAt) ||
            !readLongLong(input, entry.value.expiresAt)) {
            return false;
        }

        if (entry.value.type == data_type::String) {
            if (!readBulk(input, entry.value.value)) {
                return false;
            }
        } else if (entry.value.type == data_type::List) {
            std::size_t count = 0;
            if (!readSize(input, count)) {
                return false;
            }
            for (std::size_t j = 0; j < count; ++j) {
                std::string value;
                if (!readBulk(input, value)) {
                    return false;
                }
                entry.value.list.push_back(std::move(value));
            }
        } else if (entry.value.type == data_type::Hash) {
            std::size_t count = 0;
            if (!readSize(input, count)) {
                return false;
            }
            for (std::size_t j = 0; j < count; ++j) {
                std::string field;
                std::string value;
                if (!readBulk(input, field) || !readBulk(input, value)) {
                    return false;
                }
                entry.value.hash[std::move(field)] = std::move(value);
            }
        } else if (entry.value.type == data_type::Set) {
            std::size_t count = 0;
            if (!readSize(input, count)) {
                return false;
            }
            for (std::size_t j = 0; j < count; ++j) {
                std::string member;
                if (!readBulk(input, member)) {
                    return false;
                }
                entry.value.set.insert(std::move(member));
            }
        } else {
            return false;
        }

        entries.push_back(std::move(entry));
    }

    return true;
}

} // namespace

SnapshotPersistence::SnapshotPersistence(std::string path) : path_(std::move(path)) {}

bool SnapshotPersistence::save(DataStore& store) const {
    std::deque<DataStore> stores;
    stores.emplace_back();
    stores[0].loadSnapshot(store.snapshot());
    return save(stores);
}

bool SnapshotPersistence::save(std::deque<DataStore>& stores) const {
    const auto parent = std::filesystem::path(path_).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    const auto tempPath = path_ + ".tmp";
    std::ofstream output(tempPath, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    writeLine(output, kMultiDbMagic);
    writeLine(output, std::to_string(stores.size()));

    for (auto& store : stores) {
        if (!writeEntries(output, store.snapshot())) {
            return false;
        }
    }

    output.flush();
    if (!output.good()) {
        return false;
    }
    output.close();

    std::filesystem::rename(tempPath, path_);
    return true;
}

bool SnapshotPersistence::load(DataStore& store) const {
    std::deque<DataStore> stores(1);
    const bool loaded = load(stores);
    if (!loaded) {
        return false;
    }
    store.loadSnapshot(stores[0].snapshot());
    return true;
}

bool SnapshotPersistence::load(std::deque<DataStore>& stores) const {
    std::ifstream input(path_, std::ios::binary);
    if (!input.is_open()) {
        return true;
    }

    std::string magic;
    if (!readLine(input, magic) || (magic != kMagic && magic != kMultiDbMagic)) {
        std::cerr << "invalid snapshot file: " << path_ << "\n";
        return false;
    }

    if (magic == kMagic) {
        std::vector<SnapshotEntry> entries;
        if (!readEntries(input, entries)) {
            return false;
        }
        stores[0].loadSnapshot(entries);
        for (std::size_t i = 1; i < stores.size(); ++i) {
            stores[i].flushdb();
        }
        return true;
    }

    std::size_t databaseCount = 0;
    if (!readSize(input, databaseCount)) {
        return false;
    }

    std::vector<SnapshotEntry> entries;
    for (std::size_t i = 0; i < databaseCount; ++i) {
        if (!readEntries(input, entries)) {
            return false;
        }
        if (i < stores.size()) {
            stores[i].loadSnapshot(entries);
        }
    }

    for (std::size_t i = databaseCount; i < stores.size(); ++i) {
        stores[i].flushdb();
    }
    return true;
}

const std::string& SnapshotPersistence::path() const {
    return path_;
}

} // namespace swiftcache
