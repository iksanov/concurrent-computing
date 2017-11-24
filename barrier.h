#pragma once

#include <condition_variable>
#include <atomic>

template <class ConditionVariable = std::condition_variable>
class CyclicBarrier {
public:
    CyclicBarrier(size_t num_threads);
    void Pass();
private:
    size_t num_threads_;
    std::atomic<size_t> counter_;
    std::condition_variable cond_var_;
    std::mutex mutex_;
    size_t cycle_cnt = 0;
};

template <class ConditionVariable>
CyclicBarrier<ConditionVariable>::CyclicBarrier(size_t num_threads)
        : num_threads_(num_threads) {
    counter_.store(0);
}

template <class ConditionVariable>
void CyclicBarrier<ConditionVariable>::Pass() {
    std::unique_lock<std::mutex> lock(mutex_);
    counter_.fetch_add(1);
    if (counter_.load() == num_threads_) {
        ++cycle_cnt;
        counter_.store(0);
        cond_var_.notify_all();
    } else {
        size_t prev_cycle_cnt = cycle_cnt;
        while (counter_.load() < num_threads_ && prev_cycle_cnt == cycle_cnt) {
            cond_var_.wait(lock);
        }
    }
}
