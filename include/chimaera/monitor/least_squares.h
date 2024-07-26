//
// Created by llogan on 7/25/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_MONITOR_LEAST_SQUARES_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MONITOR_LEAST_SQUARES_H_

#include <vector>

namespace chi {

class LeastSquares {
 public:
  std::vector<std::vector<float>> data_;
  std::vector<float> consts_;
  std::string model_name_;

 public:
  void Shape(int ncol, const std::string &model_name) {
    data_.resize(ncol);
    model_name_ = model_name;
  }

  void Add(const std::vector<float> &x) {
    for (int i = 0; i < x.size(); i++) {
      data_[i].push_back(x[i]);
    }
  }

  template<typename Ar>
  void serialize(Ar &ar) {
    ar & data_;
    ar & consts_;
    ar & model_name_;
  }

  template<typename Ar>
  void deserialize(Ar &ar) {
    ar & consts_;
  }
};

}  // namespace chi

#endif //CHIMAERA_INCLUDE_CHIMAERA_MONITOR_LEAST_SQUARES_H_
