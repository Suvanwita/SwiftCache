#include "SnapshotWorker.h"

#include <iostream>

namespace swiftcache {

SnapshotWorker::SnapshotWorker(DataStore& store, SnapshotPersistence& snapshot,
                               AofPersistence* aof, std::chrono::seconds interval)
    : store_(store), snapshot_(snapshot), aof_(aof), interval_(interval) {}

SnapshotWorker::~SnapshotWorker() {
    stop();
}

void SnapshotWorker::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;
    }

    worker_ = std::thread(&SnapshotWorker::run, this);
}

void SnapshotWorker::stop() {
    running_.store(false);
    wakeup_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void SnapshotWorker::run() {
    while (running_.load()) {
        std::unique_lock<std::mutex> lock(wakeupMutex_);
        const bool stopped = wakeup_.wait_for(lock, interval_, [this]() {
            return !running_.load();
        });
        if (stopped || !running_.load()) {
            break;
        }
        lock.unlock();

        const bool saved = aof_ == nullptr
            ? snapshot_.save(store_)
            : aof_->checkpoint([this]() {
                return snapshot_.save(store_);
            });
        if (!saved) {
            std::cerr << "failed to write SwiftCache snapshot\n";
        }
    }
}

} // namespace swiftcache
