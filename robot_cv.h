#pragma once

#include <iostream>
#include <mutex>
#include <condition_variable>
#include <atomic>

class Robot {
public:
    Robot() {
        turn_.store(1);
    }
    void StepLeft() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (turn_.load() == 0) {
            cond_var_.wait(lock);
        }
        std::cout << "left" << std::endl;
        turn_.store(0);
        cond_var_.notify_all();
    }

    void StepRight() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (turn_.load() == 1) {
            cond_var_.wait(lock);
        }
        std::cout << "right" << std::endl;
        turn_.store(1);
        cond_var_.notify_all();
    }

private:
    std::mutex mutex_;
    std::atomic<size_t> turn_; // '1' is for left leg
    std::condition_variable cond_var_;
};
