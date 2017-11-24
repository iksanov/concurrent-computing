#pragma once

#include <atomic>
#include <thread>

template<typename T>
class LockFreeStack {
  struct Node {
    Node(const T &_element) : next(nullptr), element(_element) {}
    std::atomic<Node*> next;
    T element;
  };

 public:
  explicit LockFreeStack() {}
  ~LockFreeStack();
  void Push(T element);
  bool Pop(T &ret_value);

 private:
  std::atomic<Node*> prev_top_{nullptr};
  std::atomic<Node*> top_{nullptr};

  void AddToPoppedNodes(Node *node);
  void ClearOldNodes();
  void ClearActualNodes();
};

template <typename T>
LockFreeStack<T>::~LockFreeStack() {
  ClearOldNodes();
  ClearActualNodes();
}

template <typename T>
void LockFreeStack<T>::Push(T element) {
  Node *curr_top = top_.load();
  Node *new_top = new Node(element);
  new_top->next.store(curr_top);
  while (!top_.compare_exchange_strong(curr_top, new_top)) {
    new_top->next.store(curr_top);
  }
}

template <typename T>
bool LockFreeStack<T>::Pop(T &ret_value) {
  Node *curr_top = top_.load();
  while (true) {
    if (!curr_top) {
      return false;
    }
    if (top_.compare_exchange_strong(curr_top, curr_top->next.load())) {
      ret_value = curr_top->element;
      AddToPoppedNodes(curr_top);
      return true;
    }
  }
}

template <typename T>
void LockFreeStack<T>::AddToPoppedNodes(Node *node) {
  Node *curr_top = prev_top_.load();
  Node *new_old_top = node;
  new_old_top->next.store(curr_top);
  while (!prev_top_.compare_exchange_strong(curr_top, new_old_top)) {
    new_old_top->next.store(curr_top);
  }
}

template<typename T>
void LockFreeStack<T>::ClearOldNodes() {
  while (prev_top_) {
    Node *next = prev_top_.load()->next.load();
    delete prev_top_.load();
    prev_top_.store(next);
  }
}

template<typename T>
void LockFreeStack<T>::ClearActualNodes() {
  while (top_) {
    Node *next = top_.load()->next.load();
    delete top_.load();
    top_.store(next);
  }
}

template<typename T>
using ConcurrentStack = LockFreeStack<T>;
