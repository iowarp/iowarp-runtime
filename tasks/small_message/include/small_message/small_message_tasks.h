//
// Created by lukemartinlogan on 8/11/23.
//

#ifndef CHI_TASKS_SMALL_MESSAGE_INCLUDE_SMALL_MESSAGE_SMALL_MESSAGE_TASKS_H_
#define CHI_TASKS_SMALL_MESSAGE_INCLUDE_SMALL_MESSAGE_SMALL_MESSAGE_TASKS_H_

#include "chimaera/chimaera_namespace.h"

namespace chi::small_message {

CHI_NAMESPACE_INIT
#include "small_message_methods.h"

/**
 * A task to create small_message
 * */
struct CreateTaskParams {
  CLS_CONST char *lib_name_ = "small_message";

  HSHM_INLINE_CROSS_FUN
  CreateTaskParams() = default;

  HSHM_INLINE_CROSS_FUN
  CreateTaskParams(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) {}

  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void serialize(Ar &ar) {}
};
typedef chi::Admin::CreatePoolBaseTask<CreateTaskParams> CreateTask;

/** A task to destroy small_message */
typedef chi::Admin::DestroyContainerTask DestroyTask;

/**
 * A task to upgrade small_message
 * */
typedef chi::Admin::UpgradeModuleTask UpgradeTask;

/**
 * A custom task in small_message
 * */
struct MdTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN u32 depth_;
  OUT int ret_;

  /** SHM default constructor */
  HSHM_INLINE_CROSS_FUN
  explicit MdTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_INLINE_CROSS_FUN
  explicit MdTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                  const TaskNode &task_node, const PoolId &pool_id,
                  const DomainQuery &dom_query, u32 depth, chi::IntFlag flags)
      : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrioOpt::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kMd;
    task_flags_.SetBits(TASK_COROUTINE | flags);
    dom_query_ = dom_query;

    // Custom params
    depth_ = depth;
    ret_ = -1;
  }

  /** Duplicate message */
  HSHM_INLINE_CROSS_FUN
  void CopyStart(const MdTask &other, bool deep) { depth_ = other.depth_; }

  /** (De)serialize message call */
  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void SerializeStart(Ar &ar) {
    ar(depth_);
  }

  /** (De)serialize message return */
  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void SerializeEnd(Ar &ar) {
    ar(ret_);
  }
};

/**
 * A task to read, write or both
 * */
#define MD_IO_WRITE BIT_OPT(chi::IntFlag, 0)
#define MD_IO_READ BIT_OPT(chi::IntFlag, 1)
struct IoTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN hipc::Pointer data_;
  IN size_t size_;
  IN ibitfield io_flags_;
  OUT size_t ret_;

  /** SHM default constructor */
  HSHM_INLINE_CROSS_FUN
  explicit IoTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_INLINE_CROSS_FUN
  explicit IoTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                  const TaskNode &task_node, const PoolId &pool_id,
                  const DomainQuery &dom_query, size_t io_size, u32 io_flags)
      : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrioOpt::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kIo;
    task_flags_.SetBits(TASK_DATA_OWNER);
    dom_query_ = dom_query;

    // Custom params
    FullPtr<char> data = CHI_CLIENT->AllocateBuffer(HSHM_MCTX, io_size);
    data_ = data.shm_;
    size_ = io_size;
    ret_ = 0;
    io_flags_.SetBits(io_flags);
    memset(data.ptr_, 10, io_size);
  }

  /** Destructor */
  HSHM_INLINE_CROSS_FUN
  ~IoTask() {
    if (IsDataOwner()) {
      CHI_CLIENT->FreeBuffer(HSHM_MCTX, data_);
    }
  }

  /** Duplicate message */
  HSHM_INLINE_CROSS_FUN
  void CopyStart(const IoTask &other, bool deep) {
    data_ = other.data_;
    size_ = other.size_;
    io_flags_ = other.io_flags_;
  }

  /** (De)serialize message call */
  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void SerializeStart(Ar &ar) {
    ar(io_flags_);
    if (io_flags_.Any(MD_IO_WRITE)) {
      ar.bulk(DT_WRITE, data_, size_);
    } else {
      ar.bulk(DT_EXPOSE, data_, size_);
    }
  }

  /** (De)serialize message return */
  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void SerializeEnd(Ar &ar) {
    ar(io_flags_, ret_);
    if (io_flags_.Any(MD_IO_READ)) {
      ar.bulk(DT_WRITE, data_, size_);
    }
  }
};

}  // namespace chi::small_message

#endif  // CHI_TASKS_SMALL_MESSAGE_INCLUDE_SMALL_MESSAGE_SMALL_MESSAGE_TASKS_H_
