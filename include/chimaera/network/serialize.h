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

#ifndef HRUN_INCLUDE_HRUN_NETWORK_SERIALIZE_H_
#define HRUN_INCLUDE_HRUN_NETWORK_SERIALIZE_H_

#include "chimaera/chimaera_types.h"
#include "chimaera/task_registry/task.h"
#include "chimaera/api/chimaera_client.h"
#include <sstream>

namespace chm {

/**
 * Sender writes to data_
 * Receiver reads from data_
 * */
#define DT_RECEIVER_READ BIT_OPT(u32, 0)
#define DT_SENDER_WRITE BIT_OPT(u32, 0)

/**
 * Receiver will write to data_
 * Sender reads from data_
 * */
#define DT_RECEIVER_WRITE BIT_OPT(u32, 1)
#define DT_SENDER_READ BIT_OPT(u32, 1)

/** Free data_ when the data transfer is complete */
#define DT_FREE_DATA BIT_OPT(u32, 2)

/** Indicate how data should be transferred over network */
template<bool NO_XFER>
struct DataTransferBase {
  hshm::bitfield32_t flags_;  /**< Indicates how data will be accessed */
  void *data_;                /**< The virtual address of data on the node */
  size_t data_size_;          /**< The amount of data to transfer */
  DomainId node_id_;          /**< The node data is located */

  /** Serialize a data transfer object */
  template<typename Ar>
  void serialize(Ar &ar) {
    ar(flags_, (size_t)data_, data_size_, node_id_);
  }

  /** Default constructor */
  DataTransferBase() = default;

  /** Emplace constructor */
  DataTransferBase(u32 flags, void *data, size_t data_size,
                   const DomainId &node_id = DomainId::GetLocal()) :
  flags_(flags), data_(data), data_size_(data_size), node_id_(node_id) {}

  /** Copy constructor */
  DataTransferBase(const DataTransferBase &xfer) :
  flags_(xfer.flags_), data_(xfer.data_),
  data_size_(xfer.data_size_), node_id_(xfer.node_id_) {}

  /** Copy assignment */
  DataTransferBase& operator=(const DataTransferBase &xfer) {
    flags_ = xfer.flags_;
    data_ = xfer.data_;
    data_size_ = xfer.data_size_;
    node_id_ = xfer.node_id_;
    return *this;
  }

  /** Move constructor */
  DataTransferBase(DataTransferBase &&xfer) noexcept :
  flags_(xfer.flags_), data_(xfer.data_),
  data_size_(xfer.data_size_), node_id_(xfer.node_id_) {}

  /** Equality operator */
  bool operator==(const DataTransferBase &other) const {
    return flags_.bits_ == other.flags_.bits_ &&
         data_ == other.data_ &&
         data_size_ == other.data_size_ &&
         node_id_ == other.node_id_;
  }
};

using DataTransfer = DataTransferBase<true>;
using PassDataTransfer = DataTransferBase<false>;

class SegmentedTransfer {
 public:
  DomainId ret_domain_;                /**< Domain of node to return to */
  std::vector<std::pair<TaskStateId, u32>> tasks_;  /**< Task info */
  std::vector<DataTransfer> bulk_[2];   /**< Data payloads */
  std::string md_;                      /**< Metadata */

  std::string& GetMd() {
    return md_;
  }

  void AllocateSegmentsServer() {
    for (DataTransfer &xfer : bulk_[0]) {
      xfer.data_ = HRUN_CLIENT->AllocateBufferServer<TASK_YIELD_ABT>(
          xfer.data_size_).ptr_;
    }
    for (DataTransfer &xfer : bulk_[1]) {
      xfer.data_ = HRUN_CLIENT->AllocateBufferServer<TASK_YIELD_ABT>(
          xfer.data_size_).ptr_;
    }
  }

  size_t size() {
    size_t size = 0;
    for (DataTransfer &xfer : bulk_[0]) {
      size += xfer.data_size_;
    }
    for (DataTransfer &xfer : bulk_[1]) {
      size += xfer.data_size_;
    }
    size += md_.size();
    return size;
  }

  template<typename Ar>
  void serialize(Ar &ar) {
    ar(ret_domain_, tasks_, bulk_[0], bulk_[1], md_);
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

  /** Serialize using call */
  template<typename T, typename ...Args>
  BinaryOutputArchive& operator()(T &var, Args &&...args) {
    return Serialize(var, std::forward<Args>(args)...);
  }

  /** Serialize using xfer */
  BinaryOutputArchive& bulk(u32 flags,
                            hipc::Pointer &data,
                            size_t &data_size,
                            DomainId &node_id) {
    char *data_ptr = HERMES_MEMORY_MANAGER->Convert<char>(data);
    bulk(flags, data_ptr, data_size, node_id);
    return *this;
  }

  /** Serialize using xfer */
  template<typename T>
  BinaryOutputArchive& bulk(u32 flags,
                            hipc::LPointer<T> &data,
                            size_t &data_size,
                            DomainId &node_id) {
    char *data_ptr = data.ptr_;
    bulk(flags, data_ptr, data_size, node_id);
    return *this;
  }

  /** Serialize using xfer */
  BinaryOutputArchive& bulk(u32 flags,
                            char *data,
                            size_t &data_size,
                            DomainId &node_id) {
    int xfer_mode = (flags & DT_RECEIVER_READ) ? 0 : 1;
    xfer_.bulk_[xfer_mode].emplace_back(
        (DataTransfer){flags, data, data_size, node_id});
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
          xfer_.tasks_.emplace_back(var.task_state_, var.method_);
          var.template task_serialize<BinaryOutputArchive>((*this));
          if constexpr (USES_SRL_START(T)) {
            var.SerializeStart(*this);
          } else {
            var.SaveStart(*this);
          }
        } else {
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
  SegmentedTransfer xfer_;
  std::stringstream ss_;
  cereal::BinaryInputArchive ar_;
  int xfer_off_;

 public:
  /** Default constructor */
  BinaryInputArchive(SegmentedTransfer &xfer)
  : xfer_(std::move(xfer)), xfer_off_(0), ss_(), ar_(ss_) {
    ss_.str(xfer_.md_);
  }

  /** String constructor */
  BinaryInputArchive(const std::string &params) : ar_(ss_) {
    ss_.str(params);
  }

  /** Deserialize using xfer */
  BinaryInputArchive& bulk(u32 flags,
                           hipc::Pointer &data,
                           size_t &data_size,
                           DomainId &node_id) {
    char *xfer_data;
    bulk(flags, xfer_data, data_size, node_id);
    data = HERMES_MEMORY_MANAGER->Convert<void, hipc::Pointer>(xfer_data);
    return *this;
  }

  /** Deserialize using xfer */
  template<typename T>
  BinaryInputArchive& bulk(u32 flags,
                           LPointer<T> &data,
                           size_t &data_size,
                           DomainId &node_id) {
    bulk(flags, (char*&)data.ptr_, data_size, node_id);
    data.shm_ = HERMES_MEMORY_MANAGER->Convert<void, hipc::Pointer>(
        data.ptr_);
    return *this;
  }

  /** Deserialize using xfer */
  BinaryInputArchive& bulk(u32 flags,
                           char *&data,
                           size_t &data_size,
                           DomainId &node_id) {
    int xfer_mode = (flags & DT_RECEIVER_READ) ? 0 : 1;
    DataTransfer &xfer = xfer_.bulk_[xfer_off_++][xfer_mode];
    data = (char*)xfer.data_;
    data_size = xfer.data_size_;
    node_id = xfer.node_id_;
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
  HSHM_ALWAYS_INLINE
  BinaryInputArchive& Deserialize() {
    return *this;
  }
};

}  // namespace chm

#endif  // HRUN_INCLUDE_HRUN_NETWORK_SERIALIZE_H_
