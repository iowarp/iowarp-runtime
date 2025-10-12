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
 * Data transfer object for handling bulk data transfers
 * Stores hipc::FullPtr for network transfer with pointer serialization
 */
struct DataTransfer {
  hipc::FullPtr<char> data;  /**< Full pointer for transfer */
  size_t size;               /**< Size of data */
  uint32_t flags;            /**< Transfer flags (CHI_WRITE, CHI_EXPOSE) */

  DataTransfer() : size(0), flags(0) {}
  DataTransfer(const hipc::FullPtr<char>& d, size_t s, uint32_t f)
      : data(d), size(s), flags(f) {}
  DataTransfer(hipc::Pointer ptr, size_t s, uint32_t f)
      : data(hipc::FullPtr<char>(ptr)), size(s), flags(f) {}

  // Serialization support for cereal
  template<class Archive>
  void serialize(Archive& ar) {
    ar(data, size, flags);
    // FullPtr can be serialized - it stores allocator ID and offset
  }
};

/**
 * Bulk transfer metadata for handling large data transfers (deprecated - use DataTransfer)
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
class TaskLoadInArchive {
private:
  std::string data_;
  std::unique_ptr<std::istringstream> stream_;
  std::unique_ptr<cereal::BinaryInputArchive> archive_;
  std::vector<DataTransfer> data_transfers_;
  size_t task_count_;

public:
  /** Constructor from serialized data */
  explicit TaskLoadInArchive(const std::string& data)
      : data_(data),
        stream_(std::make_unique<std::istringstream>(data_)),
        archive_(std::make_unique<cereal::BinaryInputArchive>(*stream_)),
        task_count_(0) {}

  /** Constructor from const char* and size */
  TaskLoadInArchive(const char* data, size_t size)
      : data_(data, size),
        stream_(std::make_unique<std::istringstream>(data_)),
        archive_(std::make_unique<cereal::BinaryInputArchive>(*stream_)),
        task_count_(0) {}

  /** Default constructor */
  TaskLoadInArchive()
      : stream_(std::make_unique<std::istringstream>("")),
        archive_(std::make_unique<cereal::BinaryInputArchive>(*stream_)),
        task_count_(0) {}

  /** Move constructor */
  TaskLoadInArchive(TaskLoadInArchive&& other) noexcept
      : data_(std::move(other.data_)),
        stream_(std::move(other.stream_)),
        archive_(std::move(other.archive_)),
        data_transfers_(std::move(other.data_transfers_)),
        task_count_(other.task_count_) {}

  /** Move assignment operator */
  TaskLoadInArchive& operator=(TaskLoadInArchive&& other) noexcept {
    if (this != &other) {
      data_ = std::move(other.data_);
      stream_ = std::move(other.stream_);
      archive_ = std::move(other.archive_);
      data_transfers_ = std::move(other.data_transfers_);
      task_count_ = other.task_count_;
    }
    return *this;
  }

  /** Delete copy constructor and assignment */
  TaskLoadInArchive(const TaskLoadInArchive&) = delete;
  TaskLoadInArchive& operator=(const TaskLoadInArchive&) = delete;

  /** Deserialize operator for input archives */
  template<typename T>
  TaskLoadInArchive& operator>>(T& value) {
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

  /** Bulk transfer support - records transfer for later allocation */
  void bulk(hipc::Pointer& ptr, size_t size, uint32_t flags) {
    // For input archives, just record the transfer info
    // The actual buffer allocation will be done by the caller after LoadFromMessage
    // using CHI_IPC->AllocateBuffer<char>(size) and the DataTransfer info
    data_transfers_.emplace_back(ptr, size, flags);
  }

  /** Get data transfer information */
  const std::vector<DataTransfer>& GetDataTransfers() const {
    return data_transfers_;
  }

  /** Get task count */
  size_t GetTaskCount() const {
    return task_count_;
  }

  /** Load archive from BuildMessage() output */
  void LoadFromMessage(const std::string& message) {
    // Deserialize the message using cereal
    std::istringstream msg_stream(message);
    cereal::BinaryInputArchive msg_archive(msg_stream);

    // Deserialize components: task_count, archive_data, data_transfers
    std::string archive_data;
    msg_archive(task_count_);
    msg_archive(archive_data);
    msg_archive(data_transfers_);

    // Reconstruct the internal archive from the archive_data
    data_ = archive_data;
    stream_ = std::make_unique<std::istringstream>(data_);
    archive_ = std::make_unique<cereal::BinaryInputArchive>(*stream_);
  }

  /** Access underlying cereal archive */
  cereal::BinaryInputArchive& GetArchive() { return *archive_; }
};

/**
 * Archive for serializing task inputs (uses cereal::BinaryOutputArchive)  
 * Used when sending tasks to remote nodes for execution
 */
class TaskSaveInArchive {
private:
  std::ostringstream stream_;
  std::unique_ptr<cereal::BinaryOutputArchive> archive_;
  std::vector<DataTransfer> data_transfers_;
  size_t task_count_;

public:
  /** Constructor with task count - serializes task count first */
  explicit TaskSaveInArchive(size_t task_count)
      : archive_(std::make_unique<cereal::BinaryOutputArchive>(stream_)),
        task_count_(task_count) {
    // Serialize task count first
    (*archive_)(task_count_);
  }

  /** Default constructor (deprecated - use task count constructor) */
  TaskSaveInArchive()
      : archive_(std::make_unique<cereal::BinaryOutputArchive>(stream_)),
        task_count_(0) {}

  /** Move constructor */
  TaskSaveInArchive(TaskSaveInArchive&& other) noexcept
      : stream_(std::move(other.stream_)),
        archive_(std::move(other.archive_)),
        data_transfers_(std::move(other.data_transfers_)),
        task_count_(other.task_count_) {}

  /** Move assignment operator */
  TaskSaveInArchive& operator=(TaskSaveInArchive&& other) noexcept {
    if (this != &other) {
      stream_ = std::move(other.stream_);
      archive_ = std::move(other.archive_);
      data_transfers_ = std::move(other.data_transfers_);
      task_count_ = other.task_count_;
    }
    return *this;
  }

  /** Delete copy constructor and assignment */
  TaskSaveInArchive(const TaskSaveInArchive&) = delete;
  TaskSaveInArchive& operator=(const TaskSaveInArchive&) = delete;

  /** Serialize operator for output archives */
  template<typename T>
  TaskSaveInArchive& operator<<(const T& value) {
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
    // Create DataTransfer object and append to data_transfers_ vector
    DataTransfer transfer(ptr, size, flags);
    data_transfers_.push_back(transfer);
    
    // Serialize the DataTransfer object
    (*archive_)(transfer);
  }

  /** Get serialized data */
  std::string GetData() const {
    return stream_.str();
  }

  /** Get data transfer information */
  const std::vector<DataTransfer>& GetDataTransfers() const {
    return data_transfers_;
  }

  /** Get task count */
  size_t GetTaskCount() const {
    return task_count_;
  }

  /** Build message for network transfer */
  std::string BuildMessage() {
    // First, finalize the main archive by ensuring all data is flushed
    archive_.reset();  // Flush and close the archive

    // Create a new stream for the complete message
    std::ostringstream msg_stream;
    cereal::BinaryOutputArchive msg_archive(msg_stream);

    // Serialize the components:
    // 1. Task count
    msg_archive(task_count_);

    // 2. Binary archive data (as string)
    std::string archive_data = stream_.str();
    msg_archive(archive_data);

    // 3. DataTransfer vector
    msg_archive(data_transfers_);

    return msg_stream.str();
  }

  /** Access underlying cereal archive */
  cereal::BinaryOutputArchive& GetArchive() { return *archive_; }
};

/**
 * Archive for deserializing task outputs (uses cereal::BinaryInputArchive)
 * Used when receiving completed task results from remote nodes
 */
class TaskLoadOutArchive {
private:
  std::string data_;
  std::unique_ptr<std::istringstream> stream_;
  std::unique_ptr<cereal::BinaryInputArchive> archive_;
  std::vector<BulkTransferInfo> bulk_transfers_;

public:
  /** Constructor from serialized data */
  explicit TaskLoadOutArchive(const std::string& data)
      : data_(data),
        stream_(std::make_unique<std::istringstream>(data_)),
        archive_(std::make_unique<cereal::BinaryInputArchive>(*stream_)) {}

  /** Constructor from const char* and size */
  TaskLoadOutArchive(const char* data, size_t size)
      : data_(data, size),
        stream_(std::make_unique<std::istringstream>(data_)),
        archive_(std::make_unique<cereal::BinaryInputArchive>(*stream_)) {}

  /** Default constructor */
  TaskLoadOutArchive()
      : stream_(std::make_unique<std::istringstream>("")),
        archive_(std::make_unique<cereal::BinaryInputArchive>(*stream_)) {}

  /** Move constructor */
  TaskLoadOutArchive(TaskLoadOutArchive&& other) noexcept
      : data_(std::move(other.data_)),
        stream_(std::move(other.stream_)),
        archive_(std::move(other.archive_)),
        bulk_transfers_(std::move(other.bulk_transfers_)) {}

  /** Move assignment operator */
  TaskLoadOutArchive& operator=(TaskLoadOutArchive&& other) noexcept {
    if (this != &other) {
      data_ = std::move(other.data_);
      stream_ = std::move(other.stream_);
      archive_ = std::move(other.archive_);
      bulk_transfers_ = std::move(other.bulk_transfers_);
    }
    return *this;
  }

  /** Delete copy constructor and assignment */
  TaskLoadOutArchive(const TaskLoadOutArchive&) = delete;
  TaskLoadOutArchive& operator=(const TaskLoadOutArchive&) = delete;

  /** Deserialize operator for input archives */
  template<typename T>
  TaskLoadOutArchive& operator>>(T& value) {
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
class TaskSaveOutArchive {
private:
  std::ostringstream stream_;
  std::unique_ptr<cereal::BinaryOutputArchive> archive_;
  std::vector<BulkTransferInfo> bulk_transfers_;

public:
  /** Default constructor */
  TaskSaveOutArchive()
      : archive_(std::make_unique<cereal::BinaryOutputArchive>(stream_)) {}

  /** Move constructor */
  TaskSaveOutArchive(TaskSaveOutArchive&& other) noexcept
      : stream_(std::move(other.stream_)),
        archive_(std::move(other.archive_)),
        bulk_transfers_(std::move(other.bulk_transfers_)) {}

  /** Move assignment operator */
  TaskSaveOutArchive& operator=(TaskSaveOutArchive&& other) noexcept {
    if (this != &other) {
      stream_ = std::move(other.stream_);
      archive_ = std::move(other.archive_);
      bulk_transfers_ = std::move(other.bulk_transfers_);
    }
    return *this;
  }

  /** Delete copy constructor and assignment */
  TaskSaveOutArchive(const TaskSaveOutArchive&) = delete;
  TaskSaveOutArchive& operator=(const TaskSaveOutArchive&) = delete;

  /** Serialize operator for output archives */
  template<typename T>
  TaskSaveOutArchive& operator<<(const T& value) {
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