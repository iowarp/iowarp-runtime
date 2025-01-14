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

#ifndef CHI_INCLUDE_CHI_CLIENT_CHI_CLIENT_DEFN_H_
#define CHI_INCLUDE_CHI_CLIENT_CHI_CLIENT_DEFN_H_

#include <string>

#include "chimaera/network/serialize_defn.h"
#include "chimaera/queue_manager/queue_manager.h"
#include "manager.h"
#ifdef CHIMAERA_RUNTIME
#include "chimaera/work_orchestrator/work_orchestrator.h"
#endif
#include "chimaera/module_registry/task.h"

// Singleton macros
#define CHI_CLIENT hshm::Singleton<chi::Client>::GetInstance()
#define CHI_CLIENT_T chi::Client *

namespace chi {

class Client : public ConfigurationManager {
 public:
  int data_;
  hipc::atomic<u64> *unique_;
  NodeId node_id_;

 public:
  /** Default constructor */
  HSHM_INLINE_CROSS_FUN
  Client() = default;

  /** Destructor */
  HSHM_INLINE_CROSS_FUN
  ~Client() = default;

  /** Initialize the client */
  HSHM_INLINE
  Client *Create(const char *server_config_path = "",
                 const char *client_config_path = "", bool server = false) {
    hshm::ScopedMutex lock(lock_, 1);
    if (is_initialized_) {
      return this;
    }
    is_being_initialized_ = true;
    ClientInit(server_config_path, client_config_path, server);
    is_initialized_ = true;
    is_being_initialized_ = false;
    return this;
  }

/** Initialize the client (GPU) */
#ifdef CHIMAERA_RUNTIME
  HSHM_INLINE_CROSS_FUN
  void CreateGpu(hipc::AllocatorId alloc_id) {
    main_alloc_ = HSHM_MEMORY_MANAGER->GetAllocator<CHI_ALLOC_T>(alloc_id);
    data_alloc_ = nullptr;
    rdata_alloc_ = nullptr;
    HSHM_MEMORY_MANAGER->SetDefaultAllocator(main_alloc_);
    header_ = main_alloc_->GetCustomHeader<ChiShm>();
    unique_ = &header_->unique_;
    node_id_ = header_->node_id_;
    CHI_QM->ClientInit(main_alloc_, header_->queue_manager_, header_->node_id_);
    is_initialized_ = true;
    is_being_initialized_ = false;
  }
#endif

 private:
  /** Initialize client */
  HSHM_INLINE
  void ClientInit(const char *server_config_path,
                  const char *client_config_path, bool server) {
    LoadServerConfig(server_config_path);
    LoadClientConfig(client_config_path);
    LoadSharedMemory(server);
    CHI_QM->ClientInit(main_alloc_, header_->queue_manager_, header_->node_id_);
  }

 public:
  /** Connect to a Daemon's shared memory */
  void LoadSharedMemory(bool server) {
    // Load shared-memory allocator
    config::QueueManagerInfo &qm = server_config_.queue_manager_;
    auto mem_mngr = HSHM_MEMORY_MANAGER;
    if (!server) {
      mem_mngr->AttachBackend(hipc::MemoryBackendType::kPosixShmMmap,
                              qm.shm_name_);
      mem_mngr->AttachBackend(hipc::MemoryBackendType::kPosixShmMmap,
                              qm.data_shm_name_);
      mem_mngr->AttachBackend(hipc::MemoryBackendType::kPosixShmMmap,
                              qm.rdata_shm_name_);
    }
    main_alloc_ = mem_mngr->GetAllocator<CHI_ALLOC_T>(main_alloc_id_);
    data_alloc_ = mem_mngr->GetAllocator<CHI_ALLOC_T>(data_alloc_id_);
    rdata_alloc_ = mem_mngr->GetAllocator<CHI_ALLOC_T>(rdata_alloc_id_);
    mem_mngr->SetDefaultAllocator(main_alloc_);
    header_ = main_alloc_->GetCustomHeader<ChiShm>();
    unique_ = &header_->unique_;
    node_id_ = header_->node_id_;
  }

  /** Finalize Hermes explicitly */
  HSHM_INLINE_CROSS_FUN
  void Finalize() {}

  /** Create task node id */
  HSHM_INLINE_CROSS_FUN
  TaskNode MakeTaskNodeId() {
    return TaskId(header_->node_id_, unique_->fetch_add(1));
  }

  /** Create a unique ID */
  HSHM_INLINE_CROSS_FUN
  PoolId MakePoolId() {
    return PoolId(header_->node_id_, unique_->fetch_add(1));
  }

  /** Create a default-constructed task */
  template <typename TaskT, typename... Args>
  HSHM_INLINE_CROSS_FUN TaskT *NewEmptyTask(const hipc::MemContext &mctx,
                                            hipc::Pointer &p) {
    TaskT *task = main_alloc_->NewObj<TaskT>(mctx, p, main_alloc_);
    if (task == nullptr) {
      // throw std::runtime_error("Could not allocate buffer");
      HELOG(kFatal, "Could not allocate buffer (1)");
    }
    return task;
  }

  /** Create a default-constructed task */
  template <typename TaskT, typename... Args>
  HSHM_INLINE_CROSS_FUN FullPtr<TaskT> NewEmptyTask(
      const hipc::MemContext &mctx) {
    FullPtr<TaskT> task = main_alloc_->NewObjLocal<TaskT>(mctx, main_alloc_);
    if (task.shm_.IsNull()) {
      // throw std::runtime_error("Could not allocate buffer");
      HELOG(kFatal, "Could not allocate buffer (2)");
    }
    return task;
  }

  /** Construct task */
  template <typename TaskT, typename... Args>
  HSHM_INLINE_CROSS_FUN void ConstructTask(const hipc::MemContext &mctx,
                                           TaskT *task, Args &&...args) {
    hipc::Allocator::ConstructObj<TaskT>(
        *task, hipc::CtxAllocator<CHI_ALLOC_T>{main_alloc_, mctx},
        std::forward<Args>(args)...);
  }

  /** Allocate task */
  template <typename TaskT, typename... Args>
  HSHM_INLINE_CROSS_FUN hipc::FullPtr<TaskT> AllocateTask(
      const hipc::MemContext &mctx) {
    hipc::FullPtr<TaskT> task =
        main_alloc_->AllocateLocalPtr<TaskT>(mctx, sizeof(TaskT));
    if (task.shm_.IsNull()) {
      HELOG(kFatal, "Could not allocate buffer (3)");
    }
    return task;
  }

  /** Create a task */
  template <typename TaskT, typename... Args>
  HSHM_INLINE_CROSS_FUN FullPtr<TaskT> NewTask(const hipc::MemContext &mctx,
                                               const TaskNode &task_node,
                                               Args &&...args) {
    FullPtr<TaskT> ptr = main_alloc_->NewObjLocal<TaskT>(
        mctx, hipc::CtxAllocator<CHI_ALLOC_T>{main_alloc_, mctx}, task_node,
        std::forward<Args>(args)...);
    if (ptr.shm_.IsNull()) {
      // throw std::runtime_error("Could not allocate buffer");
      HELOG(kFatal, "Could not allocate buffer (4)");
    }
    return ptr;
  }

  template <typename TaskT>
  HSHM_INLINE_CROSS_FUN void MonitorTaskFrees(FullPtr<TaskT> &task) {
#ifdef CHIMAERA_TASK_DEBUG
    MonitorTaskFrees(task.ptr_);
#endif
  }

  HSHM_INLINE_CROSS_FUN
  void MonitorTaskFrees(Task *task) {
#ifdef CHIMAERA_TASK_DEBUG
    task->delcnt_++;
    if (task->delcnt_ != 1) {
      HELOG(kFatal, "Freed task {} times: node={}, state={}. method={}",
            task->delcnt_.load(), task->task_node_, task->pool_, task->method_);
    }
#endif
  }

  /** Destroy a task */
  template <typename TaskT>
  HSHM_INLINE_CROSS_FUN void DelTask(const hipc::MemContext &mctx,
                                     TaskT *task) {
#ifdef CHIMAERA_TASK_DEBUG
    MonitorTaskFrees(task);
#else
    main_alloc_->DelObj<TaskT>(mctx, task);
#endif
  }

  /** Destroy a task */
  template <typename TaskT>
  HSHM_INLINE_CROSS_FUN void DelTask(const hipc::MemContext &mctx,
                                     FullPtr<TaskT> &task) {
#ifdef CHIMAERA_TASK_DEBUG
    MonitorTaskFrees(task);
#else
    main_alloc_->DelObjLocal<TaskT>(mctx, task);
#endif
  }

#ifdef CHIMAERA_RUNTIME
  /** Destroy a task (runtime-only) */
  template <typename ContainerT, typename TaskT>
  HSHM_INLINE void DelTask(const hipc::MemContext &mctx, ContainerT *exec,
                           TaskT *task) {
#ifdef CHIMAERA_TASK_DEBUG
    MonitorTaskFrees(task);
#else
    exec->Del(mctx, task->method_, task);
#endif
  }

  /** Destroy a task (runtime-only) */
  template <typename ContainerT, typename TaskT>
  HSHM_INLINE void DelTask(const hipc::MemContext &mctx, ContainerT *exec,
                           FullPtr<TaskT> &task) {
#ifdef CHIMAERA_TASK_DEBUG
    MonitorTaskFrees(task);
#else
    exec->Del(mctx, task->method_, task);
#endif
  }
#endif

  /** Convert pointer to char* */
  template <typename T = char>
  HSHM_INLINE_CROSS_FUN T *GetMainPointer(const hipc::Pointer &p) {
    return main_alloc_->Convert<T, hipc::Pointer>(p);
  }

  /** Allocate a buffer */
  HSHM_INLINE_CROSS_FUN
  FullPtr<char> AllocateBuffer(const hipc::MemContext &mctx, size_t size) {
    return AllocateBufferSafe<false>({mctx, data_alloc_}, size);
  }

  /** Allocate a buffer (used in remote queue only) */
#ifdef CHIMAERA_RUNTIME
  HSHM_INLINE
  FullPtr<char> AllocateBufferRemote(const hipc::MemContext &mctx,
                                     size_t size) {
    return AllocateBufferSafe<true>({mctx, rdata_alloc_}, size);
  }
#endif

 private:
  /** Allocate a buffer */
  template <bool FROM_REMOTE = false>
  HSHM_INLINE_CROSS_FUN FullPtr<char> AllocateBufferSafe(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, size_t size);

 public:
  /** Free a buffer */
  HSHM_INLINE_CROSS_FUN
  void FreeBuffer(hipc::Pointer &p) {
    auto alloc = HSHM_MEMORY_MANAGER->GetAllocator<CHI_ALLOC_T>(p.alloc_id_);
    alloc->Free(hshm::ThreadId::GetNull(), p);
  }

  /** Free a buffer */
  HSHM_INLINE_CROSS_FUN
  void FreeBuffer(FullPtr<char> &p) {
    auto alloc =
        HSHM_MEMORY_MANAGER->GetAllocator<CHI_ALLOC_T>(p.shm_.alloc_id_);
    alloc->FreeLocalPtr(hshm::ThreadId::GetNull(), p);
  }

  /** Convert pointer to char* */
  template <typename T = char>
  HSHM_INLINE_CROSS_FUN T *GetDataPointer(const hipc::Pointer &p) {
    auto alloc = HSHM_MEMORY_MANAGER->GetAllocator<CHI_ALLOC_T>(p.alloc_id_);
    return alloc->Convert<T, hipc::Pointer>(p);
  }

  /** Get the queue ID */
  HSHM_INLINE_CROSS_FUN
  QueueId GetQueueId(const PoolId &id) {
    if (id == CHI_QM->process_queue_id_) {
      return CHI_QM->process_queue_id_;
    } else {
      return CHI_QM->admin_queue_id_;
    }
  }

  /** Get a queue by its ID */
  HSHM_INLINE_CROSS_FUN
  ingress::MultiQueue *GetQueue(const QueueId &queue_id) {
    QueueId real_id = GetQueueId(queue_id);
    return CHI_QM->GetQueue(real_id);
  }

#ifdef CHIMAERA_RUNTIME
  /** Performs the lpointer conversion */
  template <typename TaskT>
  void ScheduleTaskRuntime(Task *parent_task, FullPtr<TaskT> task,
                           const QueueId &ig_queue_id);
#endif
};

/** The default asynchronous method behavior */
#ifndef CHIMAERA_RUNTIME
#define CHI_TASK_METHODS(CUSTOM)                                            \
  template <typename... Args>                                               \
  HSHM_CROSS_FUN hipc::FullPtr<CUSTOM##Task> Async##CUSTOM##Alloc(          \
      const hipc::MemContext &mctx, const TaskNode &task_node,              \
      const DomainQuery &dom_query, Args &&...args) {                       \
    hipc::FullPtr<CUSTOM##Task> task = CHI_CLIENT->NewTask<CUSTOM##Task>(   \
        mctx, task_node, id_, dom_query, std::forward<Args>(args)...);      \
    return task;                                                            \
  }                                                                         \
                                                                            \
  template <typename... Args>                                               \
  HSHM_CROSS_FUN hipc::FullPtr<CUSTOM##Task> Async##CUSTOM(                 \
      const hipc::MemContext &mctx, Args &&...args) {                       \
    TaskNode task_node = CHI_CLIENT->MakeTaskNodeId();                      \
    hipc::FullPtr<CUSTOM##Task> task =                                      \
        Async##CUSTOM##Alloc(mctx, task_node, std::forward<Args>(args)...); \
    chi::ingress::MultiQueue *queue =                                       \
        CHI_CLIENT->GetQueue(CHI_QM->process_queue_id_);                    \
    queue->Emplace(chi::TaskPrioOpt::kLowLatency,                           \
                   hshm::hash<chi::DomainQuery>{}(task->dom_query_),        \
                   task.shm_);                                              \
    return task;                                                            \
  }
#else
#define CHI_TASK_METHODS(CUSTOM)                                              \
  template <typename... Args>                                                 \
  HSHM_CROSS_FUN hipc::FullPtr<CUSTOM##Task> Async##CUSTOM##Alloc(            \
      const hipc::MemContext &mctx, const TaskNode &task_node,                \
      const DomainQuery &dom_query, Args &&...args) {                         \
    hipc::FullPtr<CUSTOM##Task> task = CHI_CLIENT->NewTask<CUSTOM##Task>(     \
        mctx, task_node, id_, dom_query, std::forward<Args>(args)...);        \
    return task;                                                              \
  }                                                                           \
                                                                              \
  template <typename... Args>                                                 \
  HSHM_CROSS_FUN hipc::FullPtr<CUSTOM##Task> Async##CUSTOM(                   \
      const hipc::MemContext &mctx, Args &&...args) {                         \
    chi::Task *parent_task = CHI_CUR_TASK;                                    \
    if (parent_task) {                                                        \
      return Async##CUSTOM##Base(mctx, parent_task,                           \
                                 parent_task->task_node_ + 1,                 \
                                 std::forward<Args>(args)...);                \
    } else {                                                                  \
      return Async##CUSTOM##Base(mctx, nullptr, CHI_CLIENT->MakeTaskNodeId(), \
                                 std::forward<Args>(args)...);                \
    }                                                                         \
  }                                                                           \
                                                                              \
  template <typename... Args>                                                 \
  hipc::FullPtr<CUSTOM##Task> Async##CUSTOM##Base(                            \
      const hipc::MemContext &mctx, Task *parent_task,                        \
      const TaskNode &task_node, Args &&...args) {                            \
    hipc::FullPtr<CUSTOM##Task> task =                                        \
        Async##CUSTOM##Alloc(mctx, task_node, std::forward<Args>(args)...);   \
    CHI_CLIENT->ScheduleTaskRuntime(parent_task, task, task->pool_);          \
    return task;                                                              \
  }
#endif

/** Call duplicate if applicable */
template <typename TaskT>
constexpr inline void CALL_COPY_START(const TaskT *orig_task, TaskT *dup_task,
                                      bool deep) {
  if constexpr (TaskT::REPLICA) {
    dup_task->task_dup(*orig_task);
    if (!deep) {
      dup_task->UnsetDataOwner();
    }
    dup_task->CopyStart(*orig_task, deep);
  }
}

/** Call duplicate if applicable */
template <typename TaskT>
constexpr inline void CALL_NEW_COPY_START(const TaskT *orig_task,
                                          FullPtr<Task> &dup_task, bool deep) {
  if constexpr (TaskT::REPLICA) {
    dup_task = CHI_CLIENT->NewEmptyTask<TaskT>({}).template Cast<Task>();
    CALL_COPY_START(orig_task, (TaskT *)dup_task.ptr_, deep);
  }
}

}  // namespace chi

static HSHM_INLINE_CROSS_FUN bool CHIMAERA_CLIENT_INIT() {
#if defined(HSHM_IS_HOST)
  if (!CHI_CLIENT->IsInitialized() && !CHI_CLIENT->IsBeingInitialized() &&
      !CHI_CLIENT->IsTerminated()) {
    CHI_CLIENT->Create();
    CHI_CLIENT->is_transparent_ = true;
    return true;
  }
  return false;
#else
  return true;
#endif
}
#define TRANSPARENT_RUN CHIMAERA_CLIENT_INIT

#define HASH_TO_NODE_ID(hash) (1 + ((hash) % CHI_CLIENT->GetNumNodes()))

#endif  // CHI_INCLUDE_CHI_CLIENT_CHI_CLIENT_DEFN_H_
