//
// Created by llogan on 7/25/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_MONITOR_PYTHON_WRAPPER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MONITOR_PYTHON_WRAPPER_H_

#ifdef CHIMAERA_ENABLE_PYTHON
#include <pybind11/embed.h>
namespace py = pybind11;
#include <string>
#include "chimaera/chimaera_types.h"

namespace chi {

class PythonWrapper {
 public:
  static void RunFile(const std::string &path) {
    py::scoped_interpreter guard;
    try {
      // Execute the Python script file
      py::eval_file("script.py");
    } catch (const py::error_already_set& e) {
      py::eval_file(path.c_str());
      HELOG(kFatal, "Error executing Python script: {}", e.what());
    }
  }

  template<typename T, typename RetT>
  static RetT Run(const std::string &fname, const T& arg) {
    py::object pyfunc = py::globals()[fname.c_str()];
    py::object pyarg = py::cast(arg);
    py::object result = pyfunc(pyarg);
    return result.cast<RetT>();
  }
};

#endif

}  // namespace chi

#endif //CHIMAERA_INCLUDE_CHIMAERA_MONITOR_PYTHON_WRAPPER_H_
