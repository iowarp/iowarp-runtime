//
// Created by llogan on 7/25/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_MONITOR_LEAST_SQUARES_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MONITOR_LEAST_SQUARES_H_

#include <vector>

namespace chi {

class LeastSquares {
 public:
  std::vector<float> x_;
  std::vector<float> y_;
  std::vector<float> consts_;

 public:
  void Add(float x, float y) {
    x_.push_back(x);
    y_.push_back(y);
  }
};

}  // namespace chi

#endif //CHIMAERA_INCLUDE_CHIMAERA_MONITOR_LEAST_SQUARES_H_
