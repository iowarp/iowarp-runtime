//
// Created by llogan on 8/8/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_UTIL_LIST_QUEUE_H_
#define CHIMAERA_INCLUDE_CHIMAERA_UTIL_LIST_QUEUE_H_

#include <vector>
#include "chimaera/chimaera_types.h"

namespace chi {

template<typename T>
class ListQueue;

template<typename T>
class ListQueueIterator {
 public:
  T *entry_;

 public:
  static ListQueueIterator begin(ListQueue<T>* queue) {
    ListQueueIterator it;
    it.entry_ = queue->head_;
    return it;
  }

  static ListQueueIterator end(ListQueue<T>* queue) {
    ListQueueIterator it;
    it.entry_ = nullptr;
    return it;
  }

  ListQueueIterator &operator++() {
    entry_ = entry_->next_;
  }

  ListQueueIterator &operator--() {
    entry_ = entry_->prior_;
  }

  T* operator*() {
    return entry_;
  }

  bool operator==(const ListQueueIterator &rhs) {
    return entry_ == rhs.entry_;
  }

  bool operator!=(const ListQueueIterator &rhs) {
    return entry_ != rhs.entry_;
  }
};

template<typename T>
class ListQueue {
 public:
  T *head_ = nullptr, *tail_ = nullptr;
  size_t size_ = 0;

 public:
  void push_back(T *entry) {
    entry->next_ = nullptr;
    entry->prior_ = tail_;
    if (tail_) {
      tail_->next_ = entry;
    }
    tail_ = entry;
    if (!head_) {
      head_ = entry;
    }
  }

  void pop(T *entry) {
    if (entry->prior_) {
      entry->prior_->next_ = entry->next_;
    } else {
      head_ = entry->next_;
    }
    if (entry->next_) {
      entry->next_->prior_ = entry->prior_;
    } else {
      tail_ = entry->prior_;
    }
  }

  ListQueueIterator<T> begin() {
    return ListQueueIterator<T>::begin(this);
  }

  ListQueueIterator<T> end() {
    return ListQueueIterator<T>::end(this);
  }
};

}  // namespace chi

#endif //CHIMAERA_INCLUDE_CHIMAERA_UTIL_LIST_QUEUE_H_
