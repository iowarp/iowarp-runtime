#ifndef CHI_CHIMAERA_ADMIN_LIB_EXEC_H_
#define CHI_CHIMAERA_ADMIN_LIB_EXEC_H_

#include "chimaera/chimaera.h"

/** Execute a task */
void Run(u32 method, Task *task, RunContext &rctx) override {
  switch (method) {
    case Method::kCreate: {
      Create(reinterpret_cast<CreateTask *>(task), rctx);
      break;
    }
    case Method::kDestroy: {
      Destroy(reinterpret_cast<DestroyTask *>(task), rctx);
      break;
    }
    case Method::kCreatePool: {
      CreatePool(reinterpret_cast<CreatePoolTask *>(task), rctx);
      break;
    }
    case Method::kDestroyPool: {
      DestroyPool(reinterpret_cast<DestroyPoolTask *>(task), rctx);
      break;
    }
    case Method::kStopRuntime: {
      StopRuntime(reinterpret_cast<StopRuntimeTask *>(task), rctx);
      break;
    }
  }
}
/** Execute a task */
void Monitor(MonitorModeId mode, MethodId method, Task *task, RunContext &rctx) override {
  switch (method) {
    case Method::kCreate: {
      MonitorCreate(mode, reinterpret_cast<CreateTask *>(task), rctx);
      break;
    }
    case Method::kDestroy: {
      MonitorDestroy(mode, reinterpret_cast<DestroyTask *>(task), rctx);
      break;
    }
    case Method::kCreatePool: {
      MonitorCreatePool(mode, reinterpret_cast<CreatePoolTask *>(task), rctx);
      break;
    }
    case Method::kDestroyPool: {
      MonitorDestroyPool(mode, reinterpret_cast<DestroyPoolTask *>(task), rctx);
      break;
    }
    case Method::kStopRuntime: {
      MonitorStopRuntime(mode, reinterpret_cast<StopRuntimeTask *>(task), rctx);
      break;
    }
  }
}
/** Delete a task */
void Del(const hipc::MemContext &mctx, u32 method, Task *task) override {
  switch (method) {
    case Method::kCreate: {
      CHI_CLIENT->DelTask<CreateTask>(mctx, reinterpret_cast<CreateTask *>(task));
      break;
    }
    case Method::kDestroy: {
      CHI_CLIENT->DelTask<DestroyTask>(mctx, reinterpret_cast<DestroyTask *>(task));
      break;
    }
    case Method::kCreatePool: {
      CHI_CLIENT->DelTask<CreatePoolTask>(mctx, reinterpret_cast<CreatePoolTask *>(task));
      break;
    }
    case Method::kDestroyPool: {
      CHI_CLIENT->DelTask<DestroyPoolTask>(mctx, reinterpret_cast<DestroyPoolTask *>(task));
      break;
    }
    case Method::kStopRuntime: {
      CHI_CLIENT->DelTask<StopRuntimeTask>(mctx, reinterpret_cast<StopRuntimeTask *>(task));
      break;
    }
  }
}
/** Duplicate a task */
void CopyStart(u32 method, const Task *orig_task, Task *dup_task, bool deep) override {
  switch (method) {
    case Method::kCreate: {
      chi::CALL_COPY_START(
        reinterpret_cast<const CreateTask*>(orig_task), 
        reinterpret_cast<CreateTask*>(dup_task), deep);
      break;
    }
    case Method::kDestroy: {
      chi::CALL_COPY_START(
        reinterpret_cast<const DestroyTask*>(orig_task), 
        reinterpret_cast<DestroyTask*>(dup_task), deep);
      break;
    }
    case Method::kCreatePool: {
      chi::CALL_COPY_START(
        reinterpret_cast<const CreatePoolTask*>(orig_task), 
        reinterpret_cast<CreatePoolTask*>(dup_task), deep);
      break;
    }
    case Method::kDestroyPool: {
      chi::CALL_COPY_START(
        reinterpret_cast<const DestroyPoolTask*>(orig_task), 
        reinterpret_cast<DestroyPoolTask*>(dup_task), deep);
      break;
    }
    case Method::kStopRuntime: {
      chi::CALL_COPY_START(
        reinterpret_cast<const StopRuntimeTask*>(orig_task), 
        reinterpret_cast<StopRuntimeTask*>(dup_task), deep);
      break;
    }
  }
}
/** Duplicate a task */
void NewCopyStart(u32 method, const Task *orig_task, FullPtr<Task> &dup_task, bool deep) override {
  switch (method) {
    case Method::kCreate: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const CreateTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kDestroy: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const DestroyTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kCreatePool: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const CreatePoolTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kDestroyPool: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const DestroyPoolTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kStopRuntime: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const StopRuntimeTask*>(orig_task), dup_task, deep);
      break;
    }
  }
}
/** Serialize a task when initially pushing into remote */
void SaveStart(
    u32 method, BinaryOutputArchive<true> &ar,
    Task *task) override {
  switch (method) {
    case Method::kCreate: {
      ar << *reinterpret_cast<CreateTask*>(task);
      break;
    }
    case Method::kDestroy: {
      ar << *reinterpret_cast<DestroyTask*>(task);
      break;
    }
    case Method::kCreatePool: {
      ar << *reinterpret_cast<CreatePoolTask*>(task);
      break;
    }
    case Method::kDestroyPool: {
      ar << *reinterpret_cast<DestroyPoolTask*>(task);
      break;
    }
    case Method::kStopRuntime: {
      ar << *reinterpret_cast<StopRuntimeTask*>(task);
      break;
    }
  }
}
/** Deserialize a task when popping from remote queue */
TaskPointer LoadStart(    u32 method, BinaryInputArchive<true> &ar) override {
  TaskPointer task_ptr;
  switch (method) {
    case Method::kCreate: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<CreateTask>(
             HSHM_DEFAULT_MEM_CTX, task_ptr.shm_);
      ar >> *reinterpret_cast<CreateTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kDestroy: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<DestroyTask>(
             HSHM_DEFAULT_MEM_CTX, task_ptr.shm_);
      ar >> *reinterpret_cast<DestroyTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kCreatePool: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<CreatePoolTask>(
             HSHM_DEFAULT_MEM_CTX, task_ptr.shm_);
      ar >> *reinterpret_cast<CreatePoolTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kDestroyPool: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<DestroyPoolTask>(
             HSHM_DEFAULT_MEM_CTX, task_ptr.shm_);
      ar >> *reinterpret_cast<DestroyPoolTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kStopRuntime: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<StopRuntimeTask>(
             HSHM_DEFAULT_MEM_CTX, task_ptr.shm_);
      ar >> *reinterpret_cast<StopRuntimeTask*>(task_ptr.ptr_);
      break;
    }
  }
  return task_ptr;
}
/** Serialize a task when returning from remote queue */
void SaveEnd(u32 method, BinaryOutputArchive<false> &ar, Task *task) override {
  switch (method) {
    case Method::kCreate: {
      ar << *reinterpret_cast<CreateTask*>(task);
      break;
    }
    case Method::kDestroy: {
      ar << *reinterpret_cast<DestroyTask*>(task);
      break;
    }
    case Method::kCreatePool: {
      ar << *reinterpret_cast<CreatePoolTask*>(task);
      break;
    }
    case Method::kDestroyPool: {
      ar << *reinterpret_cast<DestroyPoolTask*>(task);
      break;
    }
    case Method::kStopRuntime: {
      ar << *reinterpret_cast<StopRuntimeTask*>(task);
      break;
    }
  }
}
/** Deserialize a task when popping from remote queue */
void LoadEnd(u32 method, BinaryInputArchive<false> &ar, Task *task) override {
  switch (method) {
    case Method::kCreate: {
      ar >> *reinterpret_cast<CreateTask*>(task);
      break;
    }
    case Method::kDestroy: {
      ar >> *reinterpret_cast<DestroyTask*>(task);
      break;
    }
    case Method::kCreatePool: {
      ar >> *reinterpret_cast<CreatePoolTask*>(task);
      break;
    }
    case Method::kDestroyPool: {
      ar >> *reinterpret_cast<DestroyPoolTask*>(task);
      break;
    }
    case Method::kStopRuntime: {
      ar >> *reinterpret_cast<StopRuntimeTask*>(task);
      break;
    }
  }
}

#endif  // CHI_CHIMAERA_ADMIN_LIB_EXEC_H_