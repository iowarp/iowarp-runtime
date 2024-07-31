//
// Created by llogan on 7/25/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_MONITOR_ROLLING_AVERAGE_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MONITOR_ROLLING_AVERAGE_H_

#include "chimaera/chimaera_types.h"
#include "model.h"

namespace chi {

class RollingAverage : public Model {
 public:
  size_t sum_;
  size_t count_;

 public:
  RollingAverage() : sum_(0), count_(0) {}

  void Shape(const std::string &name) {
    Model::TableShape(name, 1, 1000);
  }

  void Add(size_t value, const Load &predicted) {
    Model::Add({(float)value}, predicted);
    sum_ += value;
    count_++;
  }

  size_t Predict() {
    if (count_ == 0) {
      return 1;
    }
    return sum_ / count_;
  }

  void Reset() {
    sum_ = 0;
    count_ = 0;
  }
};

}  // namespace chi

#endif //CHIMAERA_INCLUDE_CHIMAERA_MONITOR_ROLLING_AVERAGE_H_
