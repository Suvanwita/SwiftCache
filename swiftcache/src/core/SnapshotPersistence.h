#pragma once

#include <string>

#include "../datastore/DataStore.h"

namespace swiftcache {

class SnapshotPersistence {
public:
    explicit SnapshotPersistence(std::string path);

    bool save(DataStore& store) const;
    bool load(DataStore& store) const;

    const std::string& path() const;

private:
    std::string path_;
};

} // namespace swiftcache
