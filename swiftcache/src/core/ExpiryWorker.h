#pragma once

#include <atomic>
#include <deque>
#include <thread>

#include "../datastore/DataStore.h"

namespace swiftcache {

class ExpiryWorker {
public:
    explicit ExpiryWorker(DataStore& store);
    explicit ExpiryWorker(std::deque<DataStore>& stores);
    ~ExpiryWorker();

    ExpiryWorker(const ExpiryWorker&) = delete;
    ExpiryWorker& operator=(const ExpiryWorker&) = delete;

    void start();
    void stop();

private:
    void run();

    DataStore* store_;
    std::deque<DataStore>* stores_;
    std::atomic<bool> running_{false};
    std::thread worker_;
};

} // namespace swiftcache
