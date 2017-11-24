#pragma once

#include <iostream>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <memory>

class Semaphore {
public:
    Semaphore(unsigned long long _cnt = 0): cnt_(_cnt) {};

    void signal() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (cnt_.fetch_add(1) == 0) {
            not_empty_cv_.notify_all();
        }
    }

    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (cnt_.load() <= 0) {
            not_empty_cv_.wait(lock);
        }
        cnt_.fetch_sub(1);
    }

private:
    std::atomic<unsigned long long> cnt_;
    std::mutex mutex_;
    std::condition_variable not_empty_cv_;
};

class Robot {
public:
    Robot() {
        left_sem_ = std::make_unique<Semaphore>(1);
        right_sem_ = std::make_unique<Semaphore>(0);
    }
    
    void StepLeft() {
        left_sem_->wait();
        std::cout << "left" << std::endl;
        right_sem_->signal();
    }

    void StepRight() {
        right_sem_->wait();
        std::cout << "right" << std::endl;
        left_sem_->signal();
    }

private:
    std::unique_ptr<Semaphore> left_sem_;
    std::unique_ptr<Semaphore> right_sem_;
};
