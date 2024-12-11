/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Hermes. The full Hermes copyright notice, including  *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the top directory. If you do not  *
 * have access to the file, you may request a copy from help@hdfgroup.org.   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef CHI_INCLUDE_CHI_NETWORK_SERIALIZE_DEFN_H_
#define CHI_INCLUDE_CHI_NETWORK_SERIALIZE_DEFN_H_

#include "chimaera/chimaera_types.h"
#include "chimaera/module_registry/task.h"
//  #include "chimaera_codegen/api/chimaera_client.h"
#include <sstream>

namespace chi {

/**
 * Sender writes to data_
 * Receiver reads from data_
 * */
#define DT_EXPOSE BIT_OPT(u32, 0)
#define DT_WRITE (BIT_OPT(u32, 1) | DT_EXPOSE)

/** Free data_ when the data transfer is complete */
#define DT_FREE_DATA BIT_OPT(u32, 2)

/** Indicate how data should be transferred over network */
template<bool NO_XFER>
struct DataTransferBase {
  hshm::bitfield32_t flags_;  /**< Indicates how data will be accessed */
  void *data_;                /**< The virtual address of data on the node */
  size_t data_size_;          /**< The amount of data to transfer */

  /** Serialize a data transfer object */
  template<typename Ar>
  void serialize(Ar &ar) {
    ar(flags_, (size_t)data_, data_size_);
  }

  /** Default constructor */
  DataTransferBase() = default;

  /** Emplace constructor */
  DataTransferBase(u32 flags, void *data, size_t data_size) :
  flags_(flags), data_(data), data_size_(data_size) {}

  /** Copy constructor */
  DataTransferBase(const DataTransferBase &xfer) :
  flags_(xfer.flags_), data_(xfer.data_),
  data_size_(xfer.data_size_) {}

  /** Copy assignment */
  DataTransferBase& operator=(const DataTransferBase &xfer) {
    flags_ = xfer.flags_;
    data_ = xfer.data_;
    data_size_ = xfer.data_size_;
    return *this;
  }

  /** Move constructor */
  DataTransferBase(DataTransferBase &&xfer) noexcept :
  flags_(xfer.flags_), data_(xfer.data_),
  data_size_(xfer.data_size_) {}

  /** Equality operator */
  bool operator==(const DataTransferBase &other) const {
    return flags_.bits_ == other.flags_.bits_ &&
         data_ == other.data_ &&
         data_size_ == other.data_size_;
  }
};

using DataTransfer = DataTransferBase<true>;
using PassDataTransfer = DataTransferBase<false>;

struct TaskSegment {
  PoolId pool_;
  u32 method_;
  size_t task_addr_;
  DomainQuery dom_;
  chi::string lib_name_;

  TaskSegment() = default;

  TaskSegment(PoolId task_state, u32 method, size_t task_addr,
              const DomainQuery &dom)
      : pool_(task_state), method_(method),
        task_addr_(task_addr), dom_(dom) {}

  TaskSegment(PoolId task_state, u32 method, size_t task_addr)
  : pool_(task_state), method_(method), task_addr_(task_addr) {}

  TaskSegment(const TaskSegment &other) = default;
  TaskSegment(TaskSegment &&other) noexcept = default;
  TaskSegment& operator=(const TaskSegment &other) = default;
  TaskSegment& operator=(TaskSegment &&other) noexcept = default;

  template<typename Ar>
  void serialize(Ar &ar) {
    ar(pool_, method_, task_addr_, dom_);
  }

  friend std::ostream& operator<<(std::ostream &os, const TaskSegment &task) {
    os << hshm::Formatter::format(
        "TaskSeg[{}]: pool={} method={} dom={}",
        task.task_addr_, task.pool_, task.method_, task.dom_);
    return os;
  }
};

class SegmentedTransfer {
 public:
  NodeId ret_node_;                  /**< The node to return to */
  std::vector<TaskSegment> tasks_;   /**< Task info */
  std::vector<DataTransfer> bulk_;   /**< Data payloads */
  std::string md_;                   /**< Metadata */

  void AllocateBulksServer();

  size_t size() const {
    size_t size = 0;
    for (const DataTransfer &xfer : bulk_) {
      size += xfer.data_size_;
    }
    size += md_.size();
    return size;
  }

  template<typename Ar>
  void serialize(Ar &ar) {
    ar(ret_node_, tasks_, bulk_, md_);
  }

  friend std::ostream& operator<<(std::ostream &os, const SegmentedTransfer &xfer) {
    for (const TaskSegment &task : xfer.tasks_) {
      os << task << std::endl;
    }
    return os;
  }
};

/** Serialize a task or task set */
template<bool is_start>
class BinaryOutputArchive {
 public:
  SegmentedTransfer xfer_;
  std::stringstream ss_;
  cereal::BinaryOutputArchive ar_;

 public:
  /** Default constructor */
  BinaryOutputArchive() : ar_(ss_) {}

  /** Copy constructor */
  BinaryOutputArchive(const BinaryOutputArchive &other) :
      xfer_(other.xfer_), ss_(other.ss_.str()), ar_(ss_) {}

  /** Serialize using call */
  template<typename T, typename ...Args>
  BinaryOutputArchive& operator()(T &var, Args &&...args) {
    return Serialize(var, std::forward<Args>(args)...);
  }

  /** Serialize using xfer */
  BinaryOutputArchive& bulk(u32 flags,
                            hipc::Pointer &data,
                            size_t &data_size) {
    char *data_ptr = HERMES_MEMORY_MANAGER->Convert<char>(data);
    bulk(flags, data_ptr, data_size);
    return *this;
  }

  /** Serialize using xfer */
  BinaryOutputArchive& bulk(u32 flags,
                            char *data,
                            size_t &data_size) {
    xfer_.bulk_.emplace_back(
        (DataTransfer){flags, data, data_size});
    return *this;
  }

  /** Serialize using left shift */
  template<typename T>
  BinaryOutputArchive& operator<<(T &var) {
    return Serialize(var);
  }

  /** Serialize using ampersand */
  template<typename T>
  BinaryOutputArchive& operator&(T &var) {
    return Serialize(var);
  }

  /** Serialize using left shift */
  template<typename T>
  BinaryOutputArchive& operator<<(T &&var) {
    return Serialize(var);
  }

  /** Serialize using ampersand */
  template<typename T>
  BinaryOutputArchive& operator&(T &&var) {
    return Serialize(var);
  }

  /** Serialize an array */
  template<typename T>
  BinaryOutputArchive& write(T *data, size_t count) {
    size_t size = count * sizeof(T);
    return Serialize(cereal::binary_data(data, size));
  }

  /** Serialize a parameter */
  template<typename T, typename ...Args>
  BinaryOutputArchive& Serialize(T &var, Args&& ...args) {
    if constexpr (IS_TASK(T)) {
      if constexpr (IS_SRL(T)) {
        if constexpr (is_start) {
          var.template task_serialize<BinaryOutputArchive>((*this));
          xfer_.tasks_.emplace_back(var.pool_, var.method_,
                                    (size_t) &var,
                                    var.dom_query_);
          if constexpr (USES_SRL_START(T)) {
            var.SerializeStart(*this);
          } else {
            var.SaveStart(*this);
          }
        } else {
          xfer_.tasks_.emplace_back(var.pool_, var.method_,
                                    var.rctx_.ret_task_addr_);
          if constexpr (USES_SRL_END(T)) {
            var.SerializeEnd(*this);
          } else {
            var.SaveEnd(*this);
          }
        }
      }
    } else {
      ar_ << var;
    }
    return Serialize(std::forward<Args>(args)...);
  }

  /** End serialization recursion */
  BinaryOutputArchive& Serialize() {
    return *this;
  }

  /** Get serialized data */
  SegmentedTransfer Get() {
    xfer_.md_ = ss_.str();
    return std::move(xfer_);
  }
};

/** Desrialize a data structure */
template<bool is_start>
class BinaryInputArchive {
 public:
  SegmentedTransfer &xfer_;
  std::stringstream ss_;
  cereal::BinaryInputArchive ar_;
  int xfer_off_;

 public:
  /** Default constructor */
  BinaryInputArchive(SegmentedTransfer &xfer)
  : xfer_(xfer), xfer_off_(0), ss_(), ar_(ss_) {
    ss_.str(xfer_.md_);
  }

  /** Deserialize using xfer */
  BinaryInputArchive& bulk(u32 flags,
                           hipc::Pointer &data,
                           size_t &data_size) {
    char *xfer_data;
    if constexpr (!is_start) {
      xfer_data = HERMES_MEMORY_MANAGER->Convert<char>(data);
    }
    bulk(flags, xfer_data, data_size);
    if constexpr (is_start) {
      data = HERMES_MEMORY_MANAGER->Convert<void, hipc::Pointer>(xfer_data);
    }
    return *this;
  }

  /** Deserialize using xfer */
  BinaryInputArchive& bulk(u32 flags,
                           char *&data,
                           size_t &data_size) {
    DataTransfer &xfer = xfer_.bulk_[xfer_off_++];
    if constexpr (is_start) {
      data = (char *) xfer.data_;
      data_size = xfer.data_size_;
    }  else {
      xfer.data_ = data;
    }
    return *this;
  }

  /** Deserialize using call */
  template<typename T, typename ...Args>
  BinaryInputArchive& operator()(T &var, Args &&...args) {
    return Deserialize(var, std::forward<Args>(args)...);
  }

  /** Deserialize using right shift */
  template<typename T>
  BinaryInputArchive& operator>>(T &var) {
    return Deserialize(var);
  }

  /** Deserialize using ampersand */
  template<typename T>
  BinaryInputArchive& operator&(T &var) {
    return Deserialize(var);
  }

  /** Deserialize an array */
  template<typename T>
  BinaryInputArchive& read(T *data, size_t count) {
    size_t size = count * sizeof(T);
    Deserialize(cereal::binary_data(data, size));
  }

  /** Deserialize a parameter */
  template<typename T, typename ...Args>
  BinaryInputArchive& Deserialize(T &var, Args&& ...args) {
    if constexpr (IS_TASK(T)) {
      if constexpr (IS_SRL(T)) {
        if constexpr (is_start) {
          var.template task_serialize<BinaryInputArchive>((*this));
          if constexpr (USES_SRL_START(T)) {
            var.SerializeStart(*this);
          } else {
            var.LoadStart(*this);
          }
        } else {
          if constexpr (USES_SRL_END(T)) {
            var.SerializeEnd(*this);
          } else {
            var.LoadEnd(*this);
          }
        }
      }
    } else {
      ar_ >> var;
    }
    return Deserialize(std::forward<Args>(args)...);
  }

  /** End deserialize recursion */
  HSHM_INLINE
  BinaryInputArchive& Deserialize() {
    return *this;
  }
};

}  // namespace chi

#endif  // CHI_INCLUDE_CHI_NETWORK_SERIALIZE_DEFN_H_
