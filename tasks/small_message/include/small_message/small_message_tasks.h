//
// Created by lukemartinlogan on 8/11/23.
//

#ifndef HRUN_TASKS_SMALL_MESSAGE_INCLUDE_SMALL_MESSAGE_SMALL_MESSAGE_TASKS_H_
#define HRUN_TASKS_SMALL_MESSAGE_INCLUDE_SMALL_MESSAGE_SMALL_MESSAGE_TASKS_H_

#include "chimaera/api/chimaera_client.h"
#include "chimaera/task_registry/task_lib.h"
#include "chimaera_admin/chimaera_admin.h"
#include "chimaera/queue_manager/queue_manager_client.h"

namespace chi::small_message {

#include "small_message_methods.h"
#include "chimaera/chimaera_namespace.h"

/**
 * A task to create small_message
 * */
using chi::Admin::CreateContainerTask;
struct CreateTask : public CreateContainerTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateTask(hipc::Allocator *alloc) : CreateContainerTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateTask(hipc::Allocator *alloc,
             const TaskNode &task_node,
             const DomainQuery &dom_query,
             const DomainQuery &affinity,
             const std::string &pool_name,
             const CreateContext &ctx)
      : CreateContainerTask(alloc, task_node, dom_query, affinity,
                            pool_name, "small_message", ctx) {
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

/** A task to destroy small_message */
using chi::Admin::DestroyContainerTask;
struct DestructTask : public DestroyContainerTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  DestructTask(hipc::Allocator *alloc) : DestroyContainerTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE
  DestructTask(hipc::Allocator *alloc,
               const TaskNode &task_node,
               const DomainQuery &dom_query,
               PoolId &pool_id)
      : DestroyContainerTask(alloc, task_node, dom_query, pool_id) {}
};

/**
 * A custom task in small_message
 * */
struct MdTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN u32 depth_;
  OUT int ret_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  MdTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  MdTask(hipc::Allocator *alloc,
         const TaskNode &task_node,
         const DomainQuery &dom_query,
         PoolId &pool_id,
         u32 depth,
         u32 flags) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kMd;
    task_flags_.SetBits(TASK_COROUTINE | flags);
    dom_query_ = dom_query;

    // Custom params
    depth_ = depth;
    ret_ = -1;
  }

  /** Duplicate message */
  void CopyStart(const MdTask &other, bool deep) {
    depth_ = other.depth_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(depth_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    ar(ret_);
  }
};

/**
 * A task to read, write or both
 * */
#define MD_IO_WRITE BIT_OPT(u32, 0)
#define MD_IO_READ BIT_OPT(u32, 1)
struct IoTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN hipc::Pointer data_;
  IN size_t size_;
  IN bitfield32_t io_flags_;
  OUT size_t ret_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  IoTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  IoTask(hipc::Allocator *alloc,
         const TaskNode &task_node,
         const DomainQuery &dom_query,
         PoolId &pool_id,
         size_t io_size,
         u32 io_flags) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kIo;
    task_flags_.SetBits(TASK_DATA_OWNER);
    dom_query_ = dom_query;

    // Custom params
    LPointer<char> data = CHI_CLIENT->AllocateBufferClient(io_size);
    data_ = data.shm_;
    size_ = io_size;
    ret_ = 0;
    io_flags_.SetBits(io_flags);
    memset(data.ptr_, 10, io_size);
  }

  /** Destructor */
  ~IoTask() {
    if (IsDataOwner()) {
      CHI_CLIENT->FreeBuffer(data_);
    }
  }

  /** Duplicate message */
  void CopyStart(const IoTask &other, bool deep) {
    data_ = other.data_;
    size_ = other.size_;
    io_flags_ = other.io_flags_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(io_flags_);
    if (io_flags_.Any(MD_IO_WRITE)) {
      ar.bulk(DT_SENDER_WRITE, data_, size_);
    }
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    ar(io_flags_, ret_);
    if (io_flags_.Any(MD_IO_READ)) {
      ar.bulk(DT_SENDER_READ, data_, size_);
    }
  }
};

}  // namespace chi

#endif  // HRUN_TASKS_SMALL_MESSAGE_INCLUDE_SMALL_MESSAGE_SMALL_MESSAGE_TASKS_H_
