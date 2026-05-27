#include "ExpiryWorker.h"

#include <chrono>

namespace swiftcache {

ExpiryWorker::ExpiryWorker(DataStore& store) : store_(&store), stores_(nullptr) {}

ExpiryWorker::ExpiryWorker(std::deque<DataStore>& stores) : store_(nullptr), stores_(&stores) {}

ExpiryWorker::~ExpiryWorker() {
    stop();
}

void ExpiryWorker::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;
    }

    worker_ = std::thread(&ExpiryWorker::run, this);
}

void ExpiryWorker::stop() {
    running_.store(false);
    if (worker_.joinable()) {
        worker_.join();
    }
}

void ExpiryWorker::run() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (stores_ != nullptr) {
            for (auto& store : *stores_) {
                store.removeExpired();
            }
        } else {
            store_->removeExpired();
        }
    }
}

} // namespace swiftcache
