#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#include "AofPersistence.h"
#include "ServerMetrics.h"
#include "SnapshotPersistence.h"
#include "../datastore/DataStore.h"

namespace swiftcache {

class SnapshotWorker {
public:
    SnapshotWorker(DataStore& store, SnapshotPersistence& snapshot, AofPersistence* aof,
                   ServerMetrics* metrics = nullptr,
                   std::chrono::seconds interval = std::chrono::seconds(30));
    SnapshotWorker(std::deque<DataStore>& stores, SnapshotPersistence& snapshot,
                   AofPersistence* aof, ServerMetrics* metrics = nullptr,
                   std::chrono::seconds interval = std::chrono::seconds(30));
    ~SnapshotWorker();

    SnapshotWorker(const SnapshotWorker&) = delete;
    SnapshotWorker& operator=(const SnapshotWorker&) = delete;

    void start();
    void stop();

private:
    void run();

    DataStore* store_;
    std::deque<DataStore>* stores_;
    SnapshotPersistence& snapshot_;
    AofPersistence* aof_;
    ServerMetrics* metrics_;
    std::chrono::seconds interval_;
    std::atomic<bool> running_{false};
    std::condition_variable wakeup_;
    std::mutex wakeupMutex_;
    std::thread worker_;
};

} // namespace swiftcache
