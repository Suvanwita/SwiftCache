#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "AofPersistence.h"
#include "SnapshotPersistence.h"
#include "../datastore/DataStore.h"

namespace swiftcache {

class SnapshotWorker {
public:
    SnapshotWorker(DataStore& store, SnapshotPersistence& snapshot, AofPersistence& aof,
                   std::chrono::seconds interval = std::chrono::seconds(30));
    ~SnapshotWorker();

    SnapshotWorker(const SnapshotWorker&) = delete;
    SnapshotWorker& operator=(const SnapshotWorker&) = delete;

    void start();
    void stop();

private:
    void run();

    DataStore& store_;
    SnapshotPersistence& snapshot_;
    AofPersistence& aof_;
    std::chrono::seconds interval_;
    std::atomic<bool> running_{false};
    std::condition_variable wakeup_;
    std::mutex wakeupMutex_;
    std::thread worker_;
};

} // namespace swiftcache
