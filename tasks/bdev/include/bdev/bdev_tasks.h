//
// Created by lukemartinlogan on 8/11/23.
//

#ifndef HRUN_TASKS_TASK_TEMPL_INCLUDE_bdev_bdev_TASKS_H_
#define HRUN_TASKS_TASK_TEMPL_INCLUDE_bdev_bdev_TASKS_H_

#include "chimaera/api/chimaera_client.h"
#include "chimaera/module_registry/module.h"
#include "chimaera_admin/chimaera_admin.h"
#include "chimaera/queue_manager/queue_manager_client.h"

namespace chi::bdev {

#include "bdev_methods.h"
#include "chimaera/chimaera_namespace.h"

/**
 * A task to create bdev
 * */
using chi::Admin::CreateContainerTask;
struct CreateTask : public CreateContainerTask {
  IN hipc::string path_;
  IN size_t size_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateTask(hipc::Allocator *alloc)
  : CreateContainerTask(alloc), path_(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateTask(hipc::Allocator *alloc,
                const TaskNode &task_node,
                const DomainQuery &dom_query,
                const DomainQuery &affinity,
                const std::string &pool_name,
                const CreateContext &ctx,
                const std::string &path,
                size_t max_size)
      : CreateContainerTask(alloc, task_node, dom_query, affinity,
                            pool_name, "bdev", ctx), path_(alloc, path) {
    // Custom params
    size_ = max_size;
  }

  HSHM_ALWAYS_INLINE
  ~CreateTask() {
    // Custom params
  }

  /** Duplicate message */
  template<typename CreateTaskT = CreateContainerTask>
  void CopyStart(const CreateTaskT &other, bool deep) {
    BaseCopyStart(other, deep);
    path_ = other.path_;
    size_ = other.size_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    BaseSerializeStart(ar);
    ar(path_, size_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    BaseSerializeEnd(ar);
  }
};

/** A task to destroy bdev */
using chi::Admin::DestroyContainerTask;
struct DestroyTask : public DestroyContainerTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  DestroyTask(hipc::Allocator *alloc)
  : DestroyContainerTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  DestroyTask(hipc::Allocator *alloc,
               const TaskNode &task_node,
               const DomainQuery &dom_query,
               PoolId &pool_id)
  : DestroyContainerTask(alloc, task_node, dom_query, pool_id) {}
};

/**
 * A custom task in bdev
 * */
struct AllocateTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN size_t size_;
  OUT size_t off_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  AllocateTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  AllocateTask(hipc::Allocator *alloc,
               const TaskNode &task_node,
               const DomainQuery &dom_query,
               const PoolId &pool_id,
               size_t size) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kAllocate;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom params
    size_ = size;
  }

  /** Duplicate message */
  void CopyStart(const AllocateTask &other, bool deep) {
    size_ = other.size_;
    off_ = other.off_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(size_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    ar(off_);
  }
};

/**
 * A custom task in bdev
 * */
struct FreeTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN size_t size_;
  IN size_t off_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  FreeTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  FreeTask(hipc::Allocator *alloc,
           const TaskNode &task_node,
           const DomainQuery &dom_query,
           const PoolId &pool_id,
           size_t size,
           size_t off) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kFree;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom params
    size_ = size;
    off_ = off;
  }

  /** Duplicate message */
  void CopyStart(const FreeTask &other, bool deep) {
    size_ = other.size_;
    off_ = other.off_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(size_, off_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {}
};

/**
 * A custom task in bdev
 * */
struct WriteTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN hipc::Pointer data_;
  IN size_t size_;
  IN size_t off_;
  OUT bool success_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  WriteTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  WriteTask(hipc::Allocator *alloc,
            const TaskNode &task_node,
            const DomainQuery &dom_query,
            const PoolId &pool_id,
            const hipc::Pointer &data,
            size_t size,
            size_t off) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kWrite;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom params
    data_ = data;
    size_ = size;
    off_ = off;
  }

  /** Duplicate message */
  void CopyStart(const WriteTask &other, bool deep) {
    data_ = other.data_;
    size_ = other.size_;
    off_ = other.off_;
    if (deep) {
      UnsetDataOwner();
    }
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar.bulk(DT_SENDER_WRITE, data_, size_);
    ar(off_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    // ar(success_);
  }
};

/**
 * A custom task in bdev
 * */
struct ReadTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN hipc::Pointer data_;
  IN size_t size_;
  IN size_t off_;
  OUT bool success_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  ReadTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  ReadTask(hipc::Allocator *alloc,
            const TaskNode &task_node,
            const DomainQuery &dom_query,
            const PoolId &pool_id,
            const hipc::Pointer &data,
            size_t size,
            size_t off) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kWrite;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom params
    data_ = data;
    size_ = size;
    off_ = off;
  }

  /** Duplicate message */
  void CopyStart(const ReadTask &other, bool deep) {
    data_ = other.data_;
    size_ = other.size_;
    off_ = other.off_;
    if (deep) {
      UnsetDataOwner();
    }
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    // ar(data_, size_, off_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    // ar(success_);
  }
};

}  // namespace chi::bdev

#endif  // HRUN_TASKS_TASK_TEMPL_INCLUDE_bdev_bdev_TASKS_H_
