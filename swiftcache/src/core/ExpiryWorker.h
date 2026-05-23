#pragma once

#include <atomic>
#include <thread>

#include "../datastore/DataStore.h"

namespace swiftcache {

class ExpiryWorker {
public:
    explicit ExpiryWorker(DataStore& store);
    ~ExpiryWorker();

    ExpiryWorker(const ExpiryWorker&) = delete;
    ExpiryWorker& operator=(const ExpiryWorker&) = delete;

    void start();
    void stop();

private:
    void run();

    DataStore& store_;
    std::atomic<bool> running_{false};
    std::thread worker_;
};

} // namespace swiftcache
