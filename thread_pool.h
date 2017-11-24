#pragma once

#include <exception>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <vector>

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

template <class T>
class ThreadPool {
 public:
  ThreadPool();
  explicit ThreadPool(const size_t num_threads);
  std::future<T> Submit(std::function<T()> task);
  void Shutdown();
  ~ThreadPool();
 private:
  bool is_shutdown_;
  size_t default_num_workers();
  size_t num_workers_;
  std::vector<std::thread> worker_threads_;
  BlockingQueue<std::packaged_task<T()>> tasks_;
};

template <class T>
size_t ThreadPool<T>::default_num_workers() {
  size_t num_workers = std::thread::hardware_concurrency();
  if (num_workers > 0) {
    return num_workers;
  }
  return 2;
}

template <class T>
ThreadPool<T>::ThreadPool()
    : is_shutdown_(false),
      num_workers_(default_num_workers()),
      worker_threads_(std::vector<std::thread>(default_num_workers())),
      tasks_(default_num_workers()) {
  for (auto& worker_thread : worker_threads_) {
    worker_thread = std::thread([&]() {
      std::packaged_task<T()> packaged_task;
      while (tasks_.Get(packaged_task)) {
        packaged_task();
      }
    });
  }
}

template <class T>
ThreadPool<T>::ThreadPool(const size_t num_threads)
    : is_shutdown_(false),
      num_workers_(num_threads),
      worker_threads_(std::vector<std::thread>(num_threads)),
      tasks_(num_threads) {
  for (auto& worker_thread : worker_threads_) {
    worker_thread = std::thread([&]() {
      std::packaged_task<T()> packaged_task;
      while (tasks_.Get(packaged_task)) {
        packaged_task();
      }
    });
  }
}

template <class T>
std::future<T> ThreadPool<T>::Submit(std::function<T()> task) {
  std::packaged_task<T()> packaged_task(task);
  std::future<T> processed = packaged_task.get_future();
  tasks_.Put(std::move(packaged_task));
  return processed;
}

template <class T>
void ThreadPool<T>::Shutdown() {
  tasks_.Shutdown();
  is_shutdown_ = true;
  for (auto& worker_thread : worker_threads_) {
    worker_thread.join();
  }
}

template <class T>
ThreadPool<T>::~ThreadPool() {
  if (!is_shutdown_) {
    Shutdown();
  }
}
