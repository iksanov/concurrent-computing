#pragma once

#include "spinlock_pause.h"

#include <atomic>
#include <thread>

template <template <typename T> class Atomic = std::atomic>
class MCSSpinLock {
 public:
  class Guard {
   public:
    explicit Guard(MCSSpinLock& spinlock)
        : spinlock_(spinlock) {
      Acquire();
    }

    ~Guard() {
      Release();
    }

   private:
    void Acquire() {
      Guard* prev_tail = spinlock_.wait_queue_tail_.exchange(this);
      if (!prev_tail) {
        is_owner_.store(true);
      } else {
        prev_tail->next_.store(this);
      }
      while(!is_owner_.load()) {
        //relax
      }
    }

    void Release() {
      if (!next_.load()) {
        auto me = this;
        if (spinlock_.wait_queue_tail_.compare_exchange_strong(me, nullptr)) {
          return ;
        }
        while(!next_.load()) {
          //relax
        }
      }
      next_.load()->is_owner_.store(true);
    }

   private:
    MCSSpinLock& spinlock_;
    Atomic<bool> is_owner_{false};
    char pad[64];
    Atomic<Guard*> next_{nullptr};
  };

 private:
  Atomic<Guard*> wait_queue_tail_{nullptr};
};

// alias for checker
template <template <typename T> class Atomic = std::atomic>
using SpinLock = MCSSpinLock<Atomic>;
