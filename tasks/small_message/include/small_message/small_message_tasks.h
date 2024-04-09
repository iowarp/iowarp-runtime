//
// Created by lukemartinlogan on 8/11/23.
//

#ifndef HRUN_TASKS_SMALL_MESSAGE_INCLUDE_SMALL_MESSAGE_SMALL_MESSAGE_TASKS_H_
#define HRUN_TASKS_SMALL_MESSAGE_INCLUDE_SMALL_MESSAGE_SMALL_MESSAGE_TASKS_H_

#include "chimaera/api/chimaera_client.h"
#include "chimaera/task_registry/task_lib.h"
#include "chimaera_admin/chimaera_admin.h"
#include "chimaera/queue_manager/queue_manager_client.h"

namespace chm::small_message {

#include "small_message_methods.h"
#include "chimaera/chimaera_namespace.h"

/**
 * A task to create small_message
 * */
using chm::Admin::CreateTaskStateTask;
struct CreateTask : public CreateTaskStateTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateTask(hipc::Allocator *alloc) : CreateTaskStateTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateTask(hipc::Allocator *alloc,
                const TaskNode &task_node,
                const DomainId &domain_id,
                const std::string &state_name,
                const TaskStateId &id)
      : CreateTaskStateTask(alloc, task_node, domain_id, state_name,
                            "small_message", id) {
  }
};

/** A task to destroy small_message */
using chm::Admin::DestroyTaskStateTask;
struct DestructTask : public DestroyTaskStateTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  DestructTask(hipc::Allocator *alloc) : DestroyTaskStateTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE
  DestructTask(hipc::Allocator *alloc,
               const TaskNode &task_node,
               const DomainId &domain_id,
               TaskStateId &state_id)
      : DestroyTaskStateTask(alloc, task_node, domain_id, state_id) {}
};

/**
 * A custom task in small_message
 * */
struct MdTask : public Task, TaskFlags<TF_SRL_SYM | TF_REPLICA> {
  IN u32 depth_;
  OUT hipc::pod_array<int, 1> ret_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  MdTask(hipc::Allocator *alloc) : Task(alloc) {
    ret_.construct(alloc, 1);
  }

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  MdTask(hipc::Allocator *alloc,
         const TaskNode &task_node,
         const DomainId &domain_id,
         TaskStateId &state_id,
         u32 lane_hash,
         u32 depth,
         u32 flags) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    lane_hash_ = lane_hash;
    prio_ = TaskPrio::kLowLatency;
    task_state_ = state_id;
    method_ = Method::kMd;
    task_flags_.SetBits(TASK_COROUTINE | flags);
    domain_id_ = domain_id;

    // Custom params
    depth_ = depth;
    ret_.construct(alloc, 1);
  }

  /** Duplicate message */
  void CopyStart(hipc::Allocator *alloc, MdTask &other) {
    task_dup(other);
  }

  /** Process duplicate message output */
  void CopyEnd(MdTask &dup_task) {
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(depth_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    ar(ret_[0]);
  }

  /** Create group */
  HSHM_ALWAYS_INLINE
  u32 GetGroup(hshm::charbuf &group) {
    return TASK_UNORDERED;
  }
};

/**
 * A custom task in small_message
 * */
struct IoTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN LPointer<char> data_;
  IN size_t size_;
  OUT int ret_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  IoTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  IoTask(hipc::Allocator *alloc,
         const TaskNode &task_node,
         const DomainId &domain_id,
         TaskStateId &state_id,
         size_t io_size) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    lane_hash_ = 3;
    prio_ = TaskPrio::kLowLatency;
    task_state_ = state_id;
    method_ = Method::kIo;
    task_flags_.SetBits(TASK_DATA_OWNER);
    domain_id_ = domain_id;

    // Custom params
    data_ = HRUN_CLIENT->AllocateBufferClient(io_size);
    size_ = io_size;
    memset(data_.ptr_, 10, io_size);
  }

  /** Destructor */
  ~IoTask() {
    if (IsDataOwner()) {
      HRUN_CLIENT->FreeBuffer(data_);
    }
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar.bulk(DT_RECEIVER_READ,
            data_, size_, domain_id_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    ar(ret_);
  }

  /** Create group */
  HSHM_ALWAYS_INLINE
  u32 GetGroup(hshm::charbuf &group) {
    return TASK_UNORDERED;
  }
};

}  // namespace chm

#endif  // HRUN_TASKS_SMALL_MESSAGE_INCLUDE_SMALL_MESSAGE_SMALL_MESSAGE_TASKS_H_
