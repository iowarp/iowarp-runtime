//
// Created by llogan on 7/27/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_MONITOR_MODEL_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MONITOR_MODEL_H_

#include <vector>

#include "chimaera/chimaera_constants.h"

namespace chi {

class Model {
 public:
  std::vector<std::vector<float>> table_;
  std::vector<Load> predicted_;
  std::atomic<size_t> tail_ = 0;
  std::atomic<size_t> head_ = 0;
  size_t ncol_ = 0;
  size_t max_samples_ = 0;
  CoMutex lock_;
  std::ofstream log_;

 public:
  void TableShape(const std::string &name, size_t ncol, size_t max_samples) {
    ncol_ = ncol;
    max_samples_ = max_samples;
    table_.resize(ncol);
    for (size_t i = 0; i < ncol; i++) {
      table_[i].resize(max_samples);
    }
    predicted_.resize(max_samples);
    std::string path = HERMES_SYSTEM_INFO->Getenv("CHIMAERA_MONITOR_OUT");
    path = hshm::ConfigParse::ExpandPath(path);
    path = hshm::Formatter::format("{}/{}.csv", path, name);
    log_ = std::ofstream(path);
  }

  void Add(const std::vector<float> &x, const Load &predicted) {
    size_t tail = tail_.fetch_add(1);
    size_t head = head_.load();
    size_t pos = tail - head;
    if (pos >= max_samples_) {
      return;  // Drop the sample
    }
    for (int i = 0; i < x.size(); i++) {
      table_[i][pos] = x[i];
    }
    predicted_[pos] = predicted;
  }

  bool DoTrain() {
    size_t head = head_.load();
    size_t tail = tail_.load();
    size_t nsamples = tail - head;
    if (nsamples > max_samples_) {
      nsamples = max_samples_;
    }
    if (tail < 10) {
      return false;
    }
    if (nsamples < 15) {
      return false;
    }
    if (nsamples * 2 >= max_samples_) {
      head_ = tail;
    }
    if (log_.is_open()) {
      for (int row = 0; row < nsamples; ++row) {
        for (int col = 0; col < table_.size(); ++col) {
          log_ << table_[col][row] << ",";
        }
        log_ << predicted_[row].cpu_load_ << std::endl;
      }
    }
    ResizeRows(nsamples);
    return true;
  }

  void ResizeRows(size_t nrow) {
    for (size_t i = 0; i < ncol_; i++) {
      table_[i].resize(nrow);
    }
  }
};

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_MONITOR_MODEL_H_
