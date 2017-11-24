#pragma once

#include "arena_allocator.h"
#include <atomic>
#include <limits>

template <typename T>
struct ElementTraits {
  static T Min() {
    return std::numeric_limits<T>::min();
  }
  static T Max() {
    return std::numeric_limits<T>::max();
  }
};

class SpinLock {
 public:
  explicit SpinLock() : locked_(false) {}

  void Lock() {
    while (locked_.test_and_set()) {}
  }

  void Unlock() {
    locked_.clear();
  }

  // adapters for BasicLockable concept
  void lock() {
    Lock();
  }

  void unlock() {
    Unlock();
  }

 private:
  std::atomic_flag locked_;
};

template <typename T>
class OptimisticLinkedSet {
 private:
  struct Node {
    T element_;
    std::atomic<Node*> next_;
    SpinLock lock_{};
    std::atomic<bool> marked_{false};

    Node(const T& element, Node* next = nullptr) : element_(element), next_(next) {}
  };

  struct Edge {
    Node* pred_;
    Node* curr_;

    Edge(Node* pred, Node* curr)
        : pred_(pred),
          curr_(curr) {}
  };

 public:
  explicit OptimisticLinkedSet(ArenaAllocator& allocator)
      : allocator_(allocator),
        size_(0) {
    CreateEmptyList();
  }

  bool Insert(const T& element) {
    while (true) {
      const Edge edge_for_insertion = Locate(element);
      std::unique_lock<SpinLock> lock_pred(edge_for_insertion.pred_->lock_);
      std::unique_lock<SpinLock> lock_curr(edge_for_insertion.curr_->lock_);
      if (Validate(edge_for_insertion)) {
        if (edge_for_insertion.curr_->element_ == element) {
          return false;
        } else {
          Node* node = allocator_.New<Node>(element);
          node->next_ = edge_for_insertion.curr_;
          edge_for_insertion.pred_->next_ = node;
          ++size_;
          return true;
        }
      }
    }
  }

  bool Remove(const T& element) {
    while (true) {
      Edge edge_for_removing = Locate(element);
      std::unique_lock<SpinLock> lock_pred(edge_for_removing.pred_->lock_);
      std::unique_lock<SpinLock> lock_curr(edge_for_removing.curr_->lock_);
      if (Validate(edge_for_removing)) {
        if (edge_for_removing.curr_->element_ != element) {
          return false;
        } else {
          edge_for_removing.curr_->marked_ = true;
          edge_for_removing.pred_->next_.store(edge_for_removing.curr_->next_);
          --size_;
          return true;
        }
      }
    }
  }

  bool Contains(const T& element) const {
    const Edge edge = Locate(element);

    return edge.curr_->element_ == element && !edge.curr_->marked_;
  }

  size_t Size() const {
    return size_;
  }

 private:
  void CreateEmptyList() {
    head_ = allocator_.New<Node>(ElementTraits<T>::Min());
    head_->next_ = allocator_.New<Node>(ElementTraits<T>::Max());
  }

  Edge Locate(const T& element) const {
    Edge edge(this->head_, head_->next_);
    while (edge.curr_->element_ < element) {
      edge.pred_ = edge.curr_;
      edge.curr_ = edge.curr_->next_;
    }
    return edge;
  }

  bool Validate(const Edge& edge) const {
    return (!((edge.pred_)->marked_) &&
        !((edge.curr_)->marked_) &&
        ((edge.pred_)->next_ == edge.curr_));
  }

 private:
  ArenaAllocator& allocator_;
  Node* head_{nullptr};
  std::atomic<size_t> size_;
};

template <typename T> using ConcurrentSet = OptimisticLinkedSet<T>;
