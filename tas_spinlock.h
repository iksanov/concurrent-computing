#pragma once

#include <atomic>
#include <mutex>

// Test-And-Set spinlock
class TASSpinLock {
 public:
  void Lock() {
    while (locked_.exchange(true, std::memory_order_acquire)) { // (1)
      std::this_thread::yield();
    }
  }

  void Unlock() {
    locked_.store(false, std::memory_order_release); // (2)
  }

 private:
  std::atomic<bool> locked_{false};
};

