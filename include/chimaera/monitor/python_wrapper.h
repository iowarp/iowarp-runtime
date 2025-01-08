//
// Created by llogan on 7/25/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_MONITOR_PYTHON_WRAPPER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MONITOR_PYTHON_WRAPPER_H_

#include <pybind11/embed.h>
#include <pybind11/stl.h>
namespace py = pybind11;
#include <string>

#include "chimaera/chimaera_types.h"

#ifdef CHIMAERA_RUNTIME
#include "chimaera/module_registry/module_registry.h"
#endif

namespace chi {

class __attribute__((visibility("hidden"))) PyOutputArchive {
 private:
  std::vector<py::object> objs_;

 public:
  /** Ampersand operator */
  template <typename T>
  void operator&(const T &arg) {
    Serialize(arg);
  }

  /** Left shift operator */
  template <typename T>
  void operator<<(const T &arg) {
    Serialize(arg);
  }

  /** Serialize to python object */
  template <typename T>
  void Serialize(const T &arg) {
    py::object obj = py::cast(arg);
    objs_.emplace_back(std::move(obj));
  }

  /** Get serialize pack */
  py::object Get() { return py::cast(objs_); }
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
  template <typename T>
  void operator&(T &arg) {
    Deserialize(arg);
  }

  /** Left shift operator */
  template <typename T>
  void operator<<(T &arg) {
    Deserialize(arg);
  }

  /** Function operator */
  template <typename T>
  void operator()(T &arg) {
    Deserialize(arg);
  }

  /** Serialize to python object */
  template <typename T>
  void Deserialize(T &arg) {
    arg = py::cast<T>(objs_[count_++]);
  }
};

class PyDataWrapper {
 public:
  virtual void ToPython(PyOutputArchive &ar) = 0;
  virtual void FromPython(PyInputArchive &iar) = 0;
};

class __attribute__((visibility("hidden"))) PythonWrapper {
 public:
  py::scoped_interpreter guard{};
  py::dict global;

 public:
  PythonWrapper() {
    global = py::globals();
#ifdef CHIMAERA_RUNTIME
    for (const std::string &lib_dir : CHI_MOD_REGISTRY->lib_dirs_) {
      RegisterPath(lib_dir);
    }
#endif
    RunString("import sys, os");
    ImportModule("chimaera_monitor");
  }

  void RegisterPath(const std::string &path) {
    RunString("sys.path.append('" + path + "')");
  }

  void ImportModule(const std::string &name) {
    RunString("from " + name + " import *");
  }

  void RunString(const std::string &script) {
    py::gil_scoped_acquire acquire;
    try {
      // Execute the Python script string
      py::exec(script.c_str(), global);
    } catch (const py::error_already_set &e) {
      HELOG(kFatal, "Error executing Python script: {}", e.what());
    }
  }

  template <typename T>
  void RunFunction(const std::string &fname, T &arg) {
    py::gil_scoped_acquire acquire;
    try {
      // Serialize the argument
      PyOutputArchive ar;
      if constexpr (std::is_base_of<PyDataWrapper, T>::value) {
        arg.ToPython(ar);
      } else {
        arg.serialize(ar);
      }
      // Run the python function
      py::object pyfunc = global[fname.c_str()];
      py::object pyarg = ar.Get();
      py::object pyresult = pyfunc(pyarg, global);
      // Deserialize the return
      PyInputArchive iar(pyresult);
      if constexpr (std::is_base_of<PyDataWrapper, T>::value) {
        arg.FromPython(iar);
      } else {
        arg.deserialize(iar);
      }
    } catch (const std::exception &e) {
      HELOG(kError, "Error running Python function: {}", e.what());
    }
  }

  template <typename T>
  void RunMethod(const std::string &class_name, const std::string &method_name,
                 T &arg) {
    // ScopedMutex lock(mutex_, 0);
    // py::gil_scoped_acquire acquire;
    try {
      // Serialize the argument
      PyOutputArchive ar;
      if constexpr (std::is_base_of<PyDataWrapper, T>::value) {
        arg.ToPython(ar);
      } else {
        arg.serialize(ar);
      }
      // Run the python function
      py::object pyclass = global[class_name.c_str()];
      py::object pymethod = pyclass.attr(method_name.c_str());
      py::object pyarg = ar.Get();
      py::object pyresult = pymethod(pyarg, global);
      // Deserialize the return
      PyInputArchive iar(pyresult);
      if constexpr (std::is_base_of<PyDataWrapper, T>::value) {
        arg.FromPython(iar);
      } else {
        arg.deserialize(iar);
      }
    } catch (const std::exception &e) {
      HELOG(kError, "Error running Python method: {}", e.what());
    }
  }
};

#define CHI_PYTHON hshm::Singleton<chi::PythonWrapper>::GetInstance()
}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_MONITOR_PYTHON_WRAPPER_H_
