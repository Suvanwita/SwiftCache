#pragma once

#include <deque>
#include <string>

#include "../datastore/DataStore.h"

namespace swiftcache {

class SnapshotPersistence {
public:
    explicit SnapshotPersistence(std::string path);

    bool save(DataStore& store) const;
    bool save(std::deque<DataStore>& stores) const;
    bool load(DataStore& store) const;
    bool load(std::deque<DataStore>& stores) const;

    const std::string& path() const;

private:
    std::string path_;
};

} // namespace swiftcache
