//
// Created by llogan on 6/10/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_UTIL_KEY_SET_H_
#define CHIMAERA_INCLUDE_CHIMAERA_UTIL_KEY_SET_H_

#include "chimaera/chimaera_types.h"

namespace chi {

template<typename T>
class KeySet {
 public:
  hshm::fixed_spsc_queue<size_t> keys_;
  std::vector<T> set_;
  size_t size_;

 public:
  void Init(size_t max_size) {
    keys_.Resize(max_size);
    set_.resize(max_size);
    for (size_t i = 0; i < max_size; ++i) {
      keys_.emplace(i);
    }
    size_ = 0;
  }

  void resize() {
    size_t old_size = set_.size();
    size_t new_size = set_.size() * 2;
    keys_.Resize(new_size);
    for (size_t i = old_size; i < new_size; ++i) {
      keys_.emplace(i);
    }
    set_.resize(new_size);
  }

  void emplace(const T &entry, size_t &key) {
    if (keys_.pop(key).IsNull()) {
      resize();
      keys_.pop(key);
    }
    set_[key] = entry;
    size_ += 1;
  }

  void peek(size_t key, T *&entry) {
    entry = &set_[key];
  }

  void pop(size_t key, T &entry) {
    entry = set_[key];
    erase(key);
  }

  void erase(size_t key) {
    keys_.emplace(key);
    size_ -= 1;
  }
};

}  // namespace chi

#endif //CHIMAERA_INCLUDE_CHIMAERA_UTIL_KEY_SET_H_
