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
    Robot(size_t _num_foots = 2): num_foots_(_num_foots) {
        vec_sem_.resize(num_foots_);
        vec_sem_[0] = std::make_unique<Semaphore>(1);
        for (size_t i = 1; i < num_foots_; ++i) {
            vec_sem_[i] = std::make_unique<Semaphore>(0);
        }
    }

    void Step(const std::size_t foot) {
        if (foot == num_foots_ - 1) {
            vec_sem_[foot]->wait();
            std::cout << "foot " << foot << std::endl;
            vec_sem_[0]->signal();
        } else {
            vec_sem_[foot]->wait();
            std::cout << "foot " << foot << std::endl;
            vec_sem_[foot+ 1]->signal();
        }
    }

private:
    size_t num_foots_ = 0;
    std::vector<std::unique_ptr<Semaphore>> vec_sem_;
};
