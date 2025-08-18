#ifndef CHIMAERA_INCLUDE_CHIMAERA_TASK_ARCHIVES_H_
#define CHIMAERA_INCLUDE_CHIMAERA_TASK_ARCHIVES_H_

#include <string>
#include <sstream>
#include <vector>
#include <memory>
#include <type_traits>

// Include cereal for serialization
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include "chimaera/types.h"

namespace chi {

// Forward declaration
class Task;

/**
 * Bulk transfer metadata for handling large data transfers
 */
struct BulkTransferInfo {
  hipc::Pointer ptr;      /**< Pointer to bulk data */
  size_t size;            /**< Size of bulk data */
  uint32_t flags;         /**< Transfer flags (CHI_WRITE, CHI_EXPOSE) */
  
  BulkTransferInfo() : size(0), flags(0) {}
  BulkTransferInfo(hipc::Pointer p, size_t s, uint32_t f) : ptr(p), size(s), flags(f) {}
};

/**
 * Archive for deserializing task inputs (uses cereal::BinaryInputArchive)
 * Used when receiving tasks from remote nodes for execution
 */
class TaskInputArchiveIN {
private:
  std::string data_;
  std::unique_ptr<std::istringstream> stream_;
  std::unique_ptr<cereal::BinaryInputArchive> archive_;
  std::vector<BulkTransferInfo> bulk_transfers_;

public:
  /** Constructor from serialized data */
  explicit TaskInputArchiveIN(const std::string& data) 
      : data_(data),
        stream_(std::make_unique<std::istringstream>(data_)),
        archive_(std::make_unique<cereal::BinaryInputArchive>(*stream_)) {}

  /** Constructor with allocator (for compatibility - allocator is unused for input archives) */
  explicit TaskInputArchiveIN(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T>& alloc)
      : TaskInputArchiveIN("") {}

  /** Constructor from const char* and size */
  TaskInputArchiveIN(const char* data, size_t size)
      : data_(data, size),
        stream_(std::make_unique<std::istringstream>(data_)),
        archive_(std::make_unique<cereal::BinaryInputArchive>(*stream_)) {}

  /** Move constructor */
  TaskInputArchiveIN(TaskInputArchiveIN&& other) noexcept
      : data_(std::move(other.data_)),
        stream_(std::move(other.stream_)),
        archive_(std::move(other.archive_)),
        bulk_transfers_(std::move(other.bulk_transfers_)) {}

  /** Move assignment operator */
  TaskInputArchiveIN& operator=(TaskInputArchiveIN&& other) noexcept {
    if (this != &other) {
      data_ = std::move(other.data_);
      stream_ = std::move(other.stream_);
      archive_ = std::move(other.archive_);
      bulk_transfers_ = std::move(other.bulk_transfers_);
    }
    return *this;
  }

  /** Delete copy constructor and assignment */
  TaskInputArchiveIN(const TaskInputArchiveIN&) = delete;
  TaskInputArchiveIN& operator=(const TaskInputArchiveIN&) = delete;

  /** Deserialize operator for input archives */
  template<typename T>
  TaskInputArchiveIN& operator>>(T& value) {
    if constexpr (std::is_base_of_v<Task, T>) {
      // Automatically call BaseSerializeIn + SerializeIn for Task-derived objects
      value.BaseSerializeIn(*this);
      value.SerializeIn(*this);
    } else {
      (*archive_)(value);
    }
    return *this;
  }

  /** Bidirectional serialization - acts as input for this archive type */
  template<typename... Args>
  void operator()(Args&... args) {
    ((*this >> args), ...);
  }

  /** Bulk transfer support */
  void bulk(hipc::Pointer ptr, size_t size, uint32_t flags) {
    bulk_transfers_.emplace_back(ptr, size, flags);
    // For input archives, we typically just record the bulk transfer info
    // The actual data transfer would be handled by the networking layer
  }

  /** Get bulk transfer information */
  const std::vector<BulkTransferInfo>& GetBulkTransfers() const {
    return bulk_transfers_;
  }

  /** Access underlying cereal archive */
  cereal::BinaryInputArchive& GetArchive() { return *archive_; }
};

/**
 * Archive for serializing task inputs (uses cereal::BinaryOutputArchive)  
 * Used when sending tasks to remote nodes for execution
 */
class TaskOutputArchiveIN {
private:
  std::ostringstream stream_;
  std::unique_ptr<cereal::BinaryOutputArchive> archive_;
  std::vector<BulkTransferInfo> bulk_transfers_;

public:
  /** Default constructor */
  TaskOutputArchiveIN() 
      : archive_(std::make_unique<cereal::BinaryOutputArchive>(stream_)) {}

  /** Constructor with allocator (for compatibility - allocator is unused for output archives) */
  explicit TaskOutputArchiveIN(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T>& alloc)
      : TaskOutputArchiveIN() {}

  /** Move constructor */
  TaskOutputArchiveIN(TaskOutputArchiveIN&& other) noexcept
      : stream_(std::move(other.stream_)),
        archive_(std::move(other.archive_)),
        bulk_transfers_(std::move(other.bulk_transfers_)) {}

  /** Move assignment operator */
  TaskOutputArchiveIN& operator=(TaskOutputArchiveIN&& other) noexcept {
    if (this != &other) {
      stream_ = std::move(other.stream_);
      archive_ = std::move(other.archive_);
      bulk_transfers_ = std::move(other.bulk_transfers_);
    }
    return *this;
  }

  /** Delete copy constructor and assignment */
  TaskOutputArchiveIN(const TaskOutputArchiveIN&) = delete;
  TaskOutputArchiveIN& operator=(const TaskOutputArchiveIN&) = delete;

  /** Serialize operator for output archives */
  template<typename T>
  TaskOutputArchiveIN& operator<<(const T& value) {
    if constexpr (std::is_base_of_v<Task, T>) {
      // Automatically call BaseSerializeIn + SerializeIn for Task-derived objects
      const_cast<T&>(value).BaseSerializeIn(*this);
      const_cast<T&>(value).SerializeIn(*this);
    } else {
      (*archive_)(value);
    }
    return *this;
  }

  /** Bidirectional serialization - acts as output for this archive type */
  template<typename... Args>
  void operator()(const Args&... args) {
    ((*this << args), ...);
  }

  /** Bulk transfer support */
  void bulk(hipc::Pointer ptr, size_t size, uint32_t flags) {
    bulk_transfers_.emplace_back(ptr, size, flags);
    // For output archives with CHI_WRITE, we record the bulk transfer
    // The actual data transfer would be handled by the networking layer
    // which can convert the pointer to actual data when needed
  }

  /** Get serialized data */
  std::string GetData() const {
    return stream_.str();
  }

  /** Get bulk transfer information */
  const std::vector<BulkTransferInfo>& GetBulkTransfers() const {
    return bulk_transfers_;
  }

  /** Access underlying cereal archive */
  cereal::BinaryOutputArchive& GetArchive() { return *archive_; }
};

/**
 * Archive for deserializing task outputs (uses cereal::BinaryInputArchive)
 * Used when receiving completed task results from remote nodes
 */
class TaskInputArchiveOUT {
private:
  std::string data_;
  std::unique_ptr<std::istringstream> stream_;
  std::unique_ptr<cereal::BinaryInputArchive> archive_;
  std::vector<BulkTransferInfo> bulk_transfers_;

public:
  /** Constructor from serialized data */
  explicit TaskInputArchiveOUT(const std::string& data)
      : data_(data),
        stream_(std::make_unique<std::istringstream>(data_)),
        archive_(std::make_unique<cereal::BinaryInputArchive>(*stream_)) {}

  /** Constructor with allocator (for compatibility - allocator is unused for input archives) */
  explicit TaskInputArchiveOUT(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T>& alloc)
      : TaskInputArchiveOUT("") {}

  /** Constructor from const char* and size */
  TaskInputArchiveOUT(const char* data, size_t size)
      : data_(data, size),
        stream_(std::make_unique<std::istringstream>(data_)),
        archive_(std::make_unique<cereal::BinaryInputArchive>(*stream_)) {}

  /** Move constructor */
  TaskInputArchiveOUT(TaskInputArchiveOUT&& other) noexcept
      : data_(std::move(other.data_)),
        stream_(std::move(other.stream_)),
        archive_(std::move(other.archive_)),
        bulk_transfers_(std::move(other.bulk_transfers_)) {}

  /** Move assignment operator */
  TaskInputArchiveOUT& operator=(TaskInputArchiveOUT&& other) noexcept {
    if (this != &other) {
      data_ = std::move(other.data_);
      stream_ = std::move(other.stream_);
      archive_ = std::move(other.archive_);
      bulk_transfers_ = std::move(other.bulk_transfers_);
    }
    return *this;
  }

  /** Delete copy constructor and assignment */
  TaskInputArchiveOUT(const TaskInputArchiveOUT&) = delete;
  TaskInputArchiveOUT& operator=(const TaskInputArchiveOUT&) = delete;

  /** Deserialize operator for input archives */
  template<typename T>
  TaskInputArchiveOUT& operator>>(T& value) {
    if constexpr (std::is_base_of_v<Task, T>) {
      // Automatically call BaseSerializeOut + SerializeOut for Task-derived objects
      value.BaseSerializeOut(*this);
      value.SerializeOut(*this);
    } else {
      (*archive_)(value);
    }
    return *this;
  }

  /** Bidirectional serialization - acts as input for this archive type */
  template<typename... Args>
  void operator()(Args&... args) {
    ((*this >> args), ...);
  }

  /** Bulk transfer support */
  void bulk(hipc::Pointer ptr, size_t size, uint32_t flags) {
    bulk_transfers_.emplace_back(ptr, size, flags);
  }

  /** Get bulk transfer information */
  const std::vector<BulkTransferInfo>& GetBulkTransfers() const {
    return bulk_transfers_;
  }

  /** Access underlying cereal archive */
  cereal::BinaryInputArchive& GetArchive() { return *archive_; }
};

/**
 * Archive for serializing task outputs (uses cereal::BinaryOutputArchive)
 * Used when sending completed task results to remote nodes
 */
class TaskOutputArchiveOUT {
private:
  std::ostringstream stream_;
  std::unique_ptr<cereal::BinaryOutputArchive> archive_;
  std::vector<BulkTransferInfo> bulk_transfers_;

public:
  /** Default constructor */
  TaskOutputArchiveOUT()
      : archive_(std::make_unique<cereal::BinaryOutputArchive>(stream_)) {}

  /** Constructor with allocator (for compatibility - allocator is unused for output archives) */
  explicit TaskOutputArchiveOUT(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T>& alloc)
      : TaskOutputArchiveOUT() {}

  /** Move constructor */
  TaskOutputArchiveOUT(TaskOutputArchiveOUT&& other) noexcept
      : stream_(std::move(other.stream_)),
        archive_(std::move(other.archive_)),
        bulk_transfers_(std::move(other.bulk_transfers_)) {}

  /** Move assignment operator */
  TaskOutputArchiveOUT& operator=(TaskOutputArchiveOUT&& other) noexcept {
    if (this != &other) {
      stream_ = std::move(other.stream_);
      archive_ = std::move(other.archive_);
      bulk_transfers_ = std::move(other.bulk_transfers_);
    }
    return *this;
  }

  /** Delete copy constructor and assignment */
  TaskOutputArchiveOUT(const TaskOutputArchiveOUT&) = delete;
  TaskOutputArchiveOUT& operator=(const TaskOutputArchiveOUT&) = delete;

  /** Serialize operator for output archives */
  template<typename T>
  TaskOutputArchiveOUT& operator<<(const T& value) {
    if constexpr (std::is_base_of_v<Task, T>) {
      // Automatically call BaseSerializeOut + SerializeOut for Task-derived objects
      const_cast<T&>(value).BaseSerializeOut(*this);
      const_cast<T&>(value).SerializeOut(*this);
    } else {
      (*archive_)(value);
    }
    return *this;
  }

  /** Bidirectional serialization - acts as output for this archive type */
  template<typename... Args>
  void operator()(const Args&... args) {
    ((*this << args), ...);
  }

  /** Bulk transfer support */
  void bulk(hipc::Pointer ptr, size_t size, uint32_t flags) {
    bulk_transfers_.emplace_back(ptr, size, flags);
    // For output archives with CHI_WRITE, we record the bulk transfer
    // The actual data transfer would be handled by the networking layer
    // which can convert the pointer to actual data when needed
  }

  /** Get serialized data */
  std::string GetData() const {
    return stream_.str();
  }

  /** Get bulk transfer information */
  const std::vector<BulkTransferInfo>& GetBulkTransfers() const {
    return bulk_transfers_;
  }

  /** Access underlying cereal archive */
  cereal::BinaryOutputArchive& GetArchive() { return *archive_; }
};

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_TASK_ARCHIVES_H_