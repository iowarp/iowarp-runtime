//
// Created by llogan on 7/25/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_MONITOR_PYTHON_WRAPPER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MONITOR_PYTHON_WRAPPER_H_

#ifdef CHIMAERA_ENABLE_PYTHON
#include <pybind11/embed.h>
#include <pybind11/stl.h>
namespace py = pybind11;
#include <string>
#include "chimaera/chimaera_types.h"

namespace chi {

class __attribute__((visibility("hidden"))) PyOutputArchive {
 private:
  std::vector<py::object> objs_;

 public:
  /** Ampersand operator */
  template<typename T>
  void operator&(const T &arg) {
    Serialize(arg);
  }

  /** Left shift operator */
  template<typename T>
  void operator<<(const T &arg) {
    Serialize(arg);
  }

  /** Serialize to python object */
  template<typename T>
  void Serialize(const T &arg) {
    py::object obj = py::cast(arg);
    objs_.emplace_back(std::move(obj));
  }

  /** Get serialize pack */
  py::object Get() {
    return py::cast(objs_);
  }
};

class __attribute__((visibility("hidden"))) PyInputArchive {
 private:
  std::vector<py::object> objs_;
  int count_ = 0;

 public:
  explicit PyInputArchive(py::object &obj) {
    if (py::isinstance<py::tuple>(obj)) {
      objs_ = obj.cast<std::vector<py::object>>();
    } else {
      objs_.push_back(obj);
    }
  }

  /** Ampersand operator */
  template<typename T>
  void operator&(T &arg) {
    Deserialize(arg);
  }

  /** Left shift operator */
  template<typename T>
  void operator<<(T &arg) {
    Deserialize(arg);
  }

  /** Function operator */
  template<typename T>
  void operator()(T &arg) {
    Deserialize(arg);
  }

  /** Serialize to python object */
  template<typename T>
  void Deserialize(T &arg) {
    arg = py::cast<T>(objs_[count_++]);
  }
};

class __attribute__((visibility("hidden"))) PythonWrapper {
 public:
  py::scoped_interpreter guard{};

 public:
  PythonWrapper() {
    RunString("import sys, os");
    RunString(
        "def hello():\n"
        "  print('Hello from python!')\n"
        "x = 42\n");
  }

  static void RunFile(const std::string &path) {
    try {
      // Execute the Python script file
      py::eval_file(path.c_str());
    } catch (const py::error_already_set& e) {
      HELOG(kFatal, "Error executing Python script: {}", e.what());
    }
  }

  static void RunString(const std::string &script) {
    try {
      // Execute the Python script string
      py::exec(script.c_str());
    } catch (const py::error_already_set& e) {
      HELOG(kFatal, "Error executing Python script: {}", e.what());
    }
  }

  template<typename T>
  static void Run(const std::string &fname, T &arg) {
    try {
      // Serialize the argument
      PyOutputArchive ar;
      arg.serialize(ar);
      // Run the python function
      py::object pyfunc = py::globals()[fname.c_str()];
      py::object pyarg = ar.Get();
      py::object pyresult = pyfunc(pyarg);
      // Deserialize the return
      PyInputArchive iar(pyresult);
      arg.deserialize(iar);
    } catch (const std::exception &e) {
      HELOG(kFatal, "Error getting Python function: {}", e.what());
    }
  }
};

#endif

}  // namespace chi

#endif //CHIMAERA_INCLUDE_CHIMAERA_MONITOR_PYTHON_WRAPPER_H_
