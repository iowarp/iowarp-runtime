//
// Created by llogan on 7/25/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_MONITOR_PYTHON_WRAPPER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MONITOR_PYTHON_WRAPPER_H_

namespace chi {

#ifdef CHIMAERA_ENABLE_PYTHON
#include <pybind11/embed.h>
namespace py = pybind11;

class PythonWrapper {
 public:
  py::scoped_interpreter guard{};

 public:
  void Exec() {
  }
};

#endif

}  // namespace chi

#endif //CHIMAERA_INCLUDE_CHIMAERA_MONITOR_PYTHON_WRAPPER_H_
