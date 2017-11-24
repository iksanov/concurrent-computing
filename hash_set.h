#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <forward_list>
#include <functional>
#include <mutex>
#include <vector>

class RWLock {
 public:
  RWLock(): rlock_(0), wlock_(0), in_writing_(false) {}

  void write_lock() {
    std::unique_lock<std::mutex> lock(_mutex);
    ++wlock_;
    while (in_writing_ || rlock_ > 0) {
      lock_cv_.wait(lock);
    }
    in_writing_ = true;
  }

  void read_lock() {
    std::unique_lock<std::mutex> lock(_mutex);
    while (wlock_ > 0) {
      lock_cv_.wait(lock);
    }
    ++rlock_;
  }

  void write_unlock() {
    std::unique_lock<std::mutex> lock(_mutex);
    in_writing_ = false;
    --wlock_;
    lock.unlock();
    lock_cv_.notify_all();
  }

  void read_unlock() {
    std::unique_lock<std::mutex> lock(_mutex);
    --rlock_;
    if (!rlock_) {
      lock_cv_.notify_all();
    }
  }

 private:
  int rlock_;
  int wlock_;
  bool in_writing_;
  std::mutex _mutex;
  std::condition_variable lock_cv_;
};

template <class T, class Hash = std::hash<T>>
class StripedHashSet {
 public:
  StripedHashSet(const size_t concurrency_level,
                 const double growth_factor = 2,
                 const double max_load_factor = 1);
  bool Insert(const T& element);
  bool Remove(const T& element);
  bool Contains(const T& element);
  size_t Size() { return size_; };

 private:
  bool check_for_elem(const T& element);
  size_t getBucketIndex(const size_t element_hash_value) { return element_hash_value % container_.size(); };
  size_t getStripeIndex(const size_t element_hash_value) { return element_hash_value % locks_.size(); };
  void rehash();
  double growth_factor_;
  double max_load_factor_;
  std::atomic<size_t> size_;
  std::vector<std::forward_list<T>> container_;
  std::vector<RWLock> locks_;
  Hash hash;
};

template <class T, class Hash>
StripedHashSet<T, Hash>::StripedHashSet(const size_t concurrency_level,
                                        const double _growthFactor,
                                        const double _maxLoadFactor)
    : growth_factor_(_growthFactor),
      max_load_factor_(_maxLoadFactor),
      size_(0) {
  locks_ = std::vector<RWLock>(concurrency_level);
  container_ = std::vector<std::forward_list<T>>(concurrency_level);
}

template <class T, class Hash>
bool StripedHashSet<T, Hash>::Insert(const T& element) {
  const size_t element_hash_value = hash(element);
  const size_t stripe_index = getStripeIndex(element_hash_value);
  locks_[stripe_index].write_lock();
  const size_t bucket_index = getBucketIndex(element_hash_value);
  if (check_for_elem(element)) {
    locks_[stripe_index].write_unlock();
    return false;
  }
  if (max_load_factor_ < size_ / container_.size()) {
    locks_[stripe_index].write_unlock();
    rehash();
    return Insert(element);
  }
  container_[bucket_index].push_front(element);
  ++size_;
  locks_[stripe_index].write_unlock();
  return true;
};

template <class T, class Hash>
bool StripedHashSet<T, Hash>::Remove(const T& element) {
  const size_t element_hash_value = hash(element);
  const size_t stripe_index = getStripeIndex(element_hash_value);
  locks_[stripe_index].write_lock();
  const size_t bucket_index = getBucketIndex(element_hash_value);
  if (!check_for_elem(element)) {
    locks_[stripe_index].write_unlock();
    return false;
  }
  container_[bucket_index].remove(element);
  --size_;
  locks_[stripe_index].write_unlock();
  return true;
};

template <class T, class Hash>
bool StripedHashSet<T, Hash>::Contains(const T& element) {
  const size_t element_hash_value = hash(element);
  const size_t stripe_index = getStripeIndex(element_hash_value);
  locks_[stripe_index].read_lock();
  bool result = check_for_elem(element);
  locks_[stripe_index].read_unlock();
  return result;
}

template <class T, class Hash>
bool StripedHashSet<T, Hash>::check_for_elem(const T& element) {
  const size_t element_hash_value = hash(element);
  const size_t bucket_index = getBucketIndex(element_hash_value);
  return std::find(container_[bucket_index].begin(), container_[bucket_index].end(), element)
      != container_[bucket_index].end();
}

template <class T, class Hash>
void StripedHashSet<T, Hash>::rehash() {
  for (auto &lock : locks_) {
    lock.write_lock();
  }
  const size_t new_size = container_.size() * growth_factor_;
  std::vector <std::forward_list<T>> new_container(new_size);
  for (auto const &bucket : container_) {
    for (auto const &item : bucket) {
      const size_t element_hash_value = hash(item);
      const size_t bucket_index = element_hash_value % new_size;
      new_container[bucket_index].push_front(std::move(item));
    }
  }
  container_ = std::move(new_container);
  for (auto &lock : locks_) {
    lock.write_unlock();
  }
}

template <typename T> using ConcurrentSet = StripedHashSet<T>;
