//
// Created by llogan on 7/25/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_MONITOR_LEAST_SQUARES_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MONITOR_LEAST_SQUARES_H_

#include "model.h"
#include "python_wrapper.h"

namespace chi {

class LeastSquares : public Model, public PyDataWrapper {
 public:
  std::vector<float> consts_;
  std::string model_name_;

 public:
  void Shape(const std::string &name,
             int n_var,
             int n_param,
             int n_out,
             const std::string &model_name) {
    TableShape(name, n_var + n_out, 1000);
    model_name_ = model_name;
    consts_.resize(n_param);
  }

  template<typename Ar>
  void serialize(Ar &ar) {
    ar & table_;
    ar & consts_;
    ar & model_name_;
  }

  template<typename Ar>
  void deserialize(Ar &ar) {
    ar & consts_;
  }

  void ToPython(PyOutputArchive &ar) {
    serialize(ar);
  }

  void FromPython(PyInputArchive &ar) {
    deserialize(ar);
  }
};

}  // namespace chi

#endif //CHIMAERA_INCLUDE_CHIMAERA_MONITOR_LEAST_SQUARES_H_
