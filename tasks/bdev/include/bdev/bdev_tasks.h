//
// Created by lukemartinlogan on 8/11/23.
//

#ifndef CHI_TASKS_TASK_TEMPL_INCLUDE_bdev_bdev_TASKS_H_
#define CHI_TASKS_TASK_TEMPL_INCLUDE_bdev_bdev_TASKS_H_

#include "chimaera/chimaera_namespace.h"
#include "chimaera/io/block_allocator.h"

namespace chi::bdev {

#include "bdev_methods.h"
CHI_NAMESPACE_INIT

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
             const PoolId &pool_id,
             const DomainQuery &dom_query,
             const DomainQuery &affinity,
             const std::string &pool_name,
             const CreateContext &ctx,
             const std::string &path,
             size_t max_size)
      : CreateContainerTask(alloc, task_node, pool_id, dom_query, affinity,
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
typedef chi::Admin::DestroyContainerTask DestroyTask;

/**
 * A custom task in bdev
 * */
struct AllocateTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN size_t size_;
  OUT size_t total_size_;
  OUT hipc::vector<Block> blocks_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  AllocateTask(hipc::Allocator *alloc)
      : Task(alloc), blocks_(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  AllocateTask(hipc::Allocator *alloc,
               const TaskNode &task_node,
               const PoolId &pool_id,
               const DomainQuery &dom_query,
               size_t size) : Task(alloc), blocks_(alloc) {
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
    blocks_ = other.blocks_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(size_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    ar(blocks_, total_size_);
  }
};

/**
 * A custom task in bdev
 * */
struct FreeTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN Block block_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  FreeTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  FreeTask(hipc::Allocator *alloc,
           const TaskNode &task_node,
           const PoolId &pool_id,
           const DomainQuery &dom_query,
           const Block &block) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kFree;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom params
    block_ = block;
  }

  /** Duplicate message */
  void CopyStart(const FreeTask &other, bool deep) {
    block_ = other.block_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(block_);
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
            const PoolId &pool_id,
            const DomainQuery &dom_query,
            const hipc::Pointer &data,
            size_t off,
            size_t size) : Task(alloc) {
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
    ar.bulk(DT_WRITE, data_, size_);
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
           const PoolId &pool_id,
           const DomainQuery &dom_query,
           const hipc::Pointer &data,
           size_t off,
           size_t size) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kRead;
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
    ar.bulk(DT_EXPOSE, data_, size_);
    ar(size_, off_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    ar.bulk(DT_WRITE, data_, size_);
    ar(success_);
  }
};

/**
 * A custom task in bdev
 * */
struct PollStatsTask : public Task, TaskFlags<TF_SRL_SYM> {
  OUT BdevStats stats_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  PollStatsTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  PollStatsTask(hipc::Allocator *alloc,
                const TaskNode &task_node,
                const PoolId &pool_id,
                const DomainQuery &dom_query,
                u32 period_ms) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    pool_ = pool_id;
    method_ = Method::kPollStats;
    if (period_ms) {
      task_flags_.SetBits(TASK_LONG_RUNNING);
      prio_ = TaskPrio::kHighLatency;
    } else {
      task_flags_.SetBits(0);
      prio_ = TaskPrio::kLowLatency;
    }
    dom_query_ = dom_query;

    SetPeriodMs(period_ms);
  }

  /** Duplicate message */
  void CopyStart(const PollStatsTask &other, bool deep) {
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    ar(stats_);
  }
};

}  // namespace chi::bdev

#endif  // CHI_TASKS_TASK_TEMPL_INCLUDE_bdev_bdev_TASKS_H_
