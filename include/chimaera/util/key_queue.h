//
// Created by llogan on 6/10/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_UTIL_KEY_QUEUE_H_
#define CHIMAERA_INCLUDE_CHIMAERA_UTIL_KEY_QUEUE_H_

#include "key_set.h"

namespace chi {

template<typename T>
class KeyQueue {
 public:
  KeySet<T> queue_;
  size_t size_, head_, tail_;
  int id_;

 public:
  void Init(int id, size_t queue_depth) {
    queue_.Init(queue_depth);
    size_ = 0;
    tail_ = 0;
    head_ = 0;
    id_ = id;
  }

  HSHM_ALWAYS_INLINE
  bool push(const T &entry) {
    size_t key;
    queue_.emplace(entry, key);
    if (size_ == 0) {
      head_ = key;
      tail_ = key;
    } else {
      T *point;
      // Tail is entry's prior
      queue_.peek(key, point);
      point->prior_ = tail_;
      // Prior's next is entry
      queue_.peek(tail_, point);
      point->next_ = key;
      // Update tail
      tail_ = key;
    }
    ++size_;
    return true;
  }

  HSHM_ALWAYS_INLINE
  void peek(T *&entry, size_t off) {
    queue_.peek(off, entry);
  }

  HSHM_ALWAYS_INLINE
  void peek(T *&entry) {
    queue_.peek(head_, entry);
  }

  HSHM_ALWAYS_INLINE
  void pop(T &entry) {
    T *point;
    peek(point);
    entry = *point;
    erase();
  }

  HSHM_ALWAYS_INLINE
  void erase() {
    T *point;
    queue_.peek(head_, point);
    size_t head = point->next_;
    queue_.erase(head_);
    head_ = head;
    --size_;
  }
};


}  // namespace chi

#endif //CHIMAERA_INCLUDE_CHIMAERA_UTIL_KEY_QUEUE_H_
