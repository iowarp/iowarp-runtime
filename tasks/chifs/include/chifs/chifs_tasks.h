//
// Created by lukemartinlogan on 8/11/23.
//

#ifndef CHI_TASKS_TASK_TEMPL_INCLUDE_chifs_chifs_TASKS_H_
#define CHI_TASKS_TASK_TEMPL_INCLUDE_chifs_chifs_TASKS_H_

#include "chimaera/api/chimaera_client.h"
#include "chimaera/module_registry/module.h"
#include "chimaera_admin/chimaera_admin.h"
#include "chimaera/queue_manager/queue_manager_client.h"
#include "chifs_types.h"

namespace chi::chifs {

#include "chifs_methods.h"
#include "chimaera/chimaera_namespace.h"

/**
 * A task to create chifs
 * */
using chi::Admin::CreateContainerTask;
struct CreateTask : public CreateContainerTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateTask(hipc::Allocator *alloc)
  : CreateContainerTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateTask(hipc::Allocator *alloc,
                const TaskNode &task_node,
                const DomainQuery &dom_query,
                const DomainQuery &affinity,
                const std::string &pool_name,
                const CreateContext &ctx)
      : CreateContainerTask(alloc, task_node, dom_query, affinity,
                            pool_name, "chifs", ctx) {
    // Custom params
  }

  HSHM_ALWAYS_INLINE
  ~CreateTask() {
    // Custom params
  }

  /** Duplicate message */
  template<typename CreateTaskT = CreateContainerTask>
  void CopyStart(const CreateTaskT &other, bool deep) {
    BaseCopyStart(other, deep);
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    BaseSerializeStart(ar);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    BaseSerializeEnd(ar);
  }
};

/** A task to destroy chifs */
typedef chi::Admin::DestroyContainerTask DestroyTask;

/**
 * Open a file chifs
 * */
struct OpenFileTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN hipc::string filename_;
  IN int oflags_;
  OUT FileId file_id_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  OpenFileTask(hipc::Allocator *alloc)
  : Task(alloc), filename_(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  OpenFileTask(hipc::Allocator *alloc,
               const TaskNode &task_node,
               const DomainQuery &dom_query,
               const PoolId &pool_id,
               const std::string &filename,
               int oflags) : Task(alloc), filename_(alloc, filename) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kOpenFile;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom params
    oflags_ = oflags;
  }

  /** Duplicate message */
  void CopyStart(const OpenFileTask &other, bool deep) {
    filename_ = other.filename_;
    oflags_ = other.oflags_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(filename_, oflags_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    ar(file_id_);
  }
};

/**
 * Write a file chifs
 * */
struct WriteFileTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN FileId file_id_;
  IN hipc::Pointer data_;
  IN size_t data_size_;
  IN size_t file_off_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  WriteFileTask(hipc::Allocator *alloc)
  : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  WriteFileTask(hipc::Allocator *alloc,
               const TaskNode &task_node,
               const DomainQuery &dom_query,
               const PoolId &pool_id,
               const FileId &file_id,
               const hipc::Pointer &data,
               size_t data_size,
               size_t file_off) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kWriteFile;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom params
    file_id_ = file_id;
    data_ = data;
    data_size_ = data_size;
    file_off_ = file_off;
  }

  /** Duplicate message */
  void CopyStart(const WriteFileTask &other, bool deep) {
    file_id_ = other.file_id_;
    data_ = other.data_;
    data_size_ = other.data_size_;
    file_off_ = other.file_off_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(file_id_, data_, data_size_, file_off_);
    ar.bulk(DT_SENDER_WRITE, data_, data_size_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
  }
};

/**
 * Read a file chifs
 * */
struct ReadFileTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN FileId file_id_;
  IN hipc::Pointer data_;
  IN size_t data_size_;
  IN size_t file_off_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  ReadFileTask(hipc::Allocator *alloc)
  : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  ReadFileTask(hipc::Allocator *alloc,
                const TaskNode &task_node,
                const DomainQuery &dom_query,
                const PoolId &pool_id,
                const FileId &file_id,
                const hipc::Pointer &data,
                size_t data_size,
                size_t file_off) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kReadFile;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom params
    file_id_ = file_id;
    data_ = data;
    data_size_ = data_size;
    file_off_ = file_off;
  }

  /** Duplicate message */
  void CopyStart(const WriteFileTask &other, bool deep) {
    file_id_ = other.file_id_;
    data_ = other.data_;
    data_size_ = other.data_size_;
    file_off_ = other.file_off_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(file_id_, data_size_, file_off_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    ar.bulk(DT_SENDER_READ, data_, data_size_);
  }
};


/**
 * Destroy a file chifs
 * */
struct DestroyFileTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN FileId file_id_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  DestroyFileTask(hipc::Allocator *alloc)
      : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  DestroyFileTask(hipc::Allocator *alloc,
               const TaskNode &task_node,
               const DomainQuery &dom_query,
               const PoolId &pool_id,
               const FileId &file_id) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kDestroyFile;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom params
    file_id_ = file_id;
  }

  /** Duplicate message */
  void CopyStart(const WriteFileTask &other, bool deep) {
    file_id_ = other.file_id_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(file_id_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
  }
};


/**
 * Stat a file chifs
 * */
struct StatFileTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN FileId file_id_;
  OUT size_t file_size_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  StatFileTask(hipc::Allocator *alloc)
      : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  StatFileTask(hipc::Allocator *alloc,
                  const TaskNode &task_node,
                  const DomainQuery &dom_query,
                  const PoolId &pool_id,
                  const FileId &file_id) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kDestroyFile;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom params
    file_id_ = file_id;
  }

  /** Duplicate message */
  void CopyStart(const WriteFileTask &other, bool deep) {
    file_id_ = other.file_id_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(file_id_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    ar(file_size_);
  }
};


}  // namespace chi::chifs

#endif  // CHI_TASKS_TASK_TEMPL_INCLUDE_chifs_chifs_TASKS_H_
