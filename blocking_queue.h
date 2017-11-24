#pragma once

#include <exception>
#include <deque>
#include <mutex>
#include <condition_variable>

class ShutdownQueueException: public std::exception {};

template <class T, class Container = std::deque<T>>
class BlockingQueue {
 public:
  explicit BlockingQueue(const size_t& capacity);
  void Put(T &&element);
  bool Get(T &result);
  void Shutdown();
 private:
  Container container_;
  size_t capacity_;
  bool is_shutdown_;
  std::mutex mutex_;
  std::condition_variable cv_put_;
  std::condition_variable cv_get_;
};

template <class T, class Container>
BlockingQueue<T, Container>::BlockingQueue(const size_t& capacity)
    : capacity_(capacity), is_shutdown_(false) {}

template <class T, class Container>
void BlockingQueue<T, Container>::Put(T &&element) {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_put_.wait(lock, [&]() { return  container_.size() < capacity_ || is_shutdown_; });
  if (is_shutdown_) {
    throw ShutdownQueueException();
  }
  container_.push_back(std::move(element));
  cv_get_.notify_one();
}

template <class T, class Container>
bool BlockingQueue<T, Container>::Get(T &result) {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_get_.wait(lock, [&]() { return !container_.empty() || is_shutdown_; });
  if (container_.empty()) {
    return false;
  }
  result = std::move(container_.front());
  container_.pop_front();
  cv_put_.notify_one();
  return true;
}

template <class T, class Container>
void BlockingQueue<T, Container>::Shutdown() {
  std::unique_lock<std::mutex> lock(mutex_);
  is_shutdown_ = true;
  cv_put_.notify_all();
  cv_get_.notify_all();
}
