//
// Created by llogan on 7/27/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_MONITOR_MODEL_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MONITOR_MODEL_H_

#include <vector>

namespace chi {

class Model {
 public:
  std::vector<std::vector<float>> table_;
  std::atomic<size_t> tail_ = 0;
  std::atomic<size_t> head_ = 0;
  size_t ncol_ = 0;
  size_t max_samples_ = 0;
  CoMutex lock_;

 public:
  void TableShape(size_t ncol, size_t max_samples) {
    max_samples_ = max_samples;
    table_.resize(ncol);
    for (size_t i = 0; i < ncol; i++) {
      table_[i].resize(max_samples);
      table_[i].resize(0);
    }
  }

  void Add(const std::vector<float> &x) {
    size_t tail = tail_.fetch_add(x.size());
    size_t head = head_.load();
    size_t pos = tail - head;
    if (pos >= max_samples_) {
      return;  // Drop the sample
    }
    for (int i = 0; i < x.size(); i++) {
      table_[i][pos] = x[i];
    }
  }

  bool BeginTrain() {
    size_t head = head_.load();
    size_t tail = tail_.load();
    size_t pos = tail - head;
    if (tail < 10) {
      return false;
    }
    if (tail < head * 11 / 10) {
      return false;
    }
    if (pos * 2 >= max_samples_) {
      head_ = tail;
    }
    ResizeRows(pos);
    return true;
  }

  void EndTrain() {
  }

  void ResizeRows(size_t nrow) {
    for (size_t i = 0; i < ncol_; i++) {
      table_[i].resize(nrow);
    }
  }
};

}  // namespace chi

#endif //CHIMAERA_INCLUDE_CHIMAERA_MONITOR_MODEL_H_
