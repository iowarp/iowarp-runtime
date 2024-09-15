#ifndef CHI_BDEV_LIB_EXEC_H_
#define CHI_BDEV_LIB_EXEC_H_

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
    case Method::kAllocate: {
      Allocate(reinterpret_cast<AllocateTask *>(task), rctx);
      break;
    }
    case Method::kFree: {
      Free(reinterpret_cast<FreeTask *>(task), rctx);
      break;
    }
    case Method::kWrite: {
      Write(reinterpret_cast<WriteTask *>(task), rctx);
      break;
    }
    case Method::kRead: {
      Read(reinterpret_cast<ReadTask *>(task), rctx);
      break;
    }
    case Method::kPollStats: {
      PollStats(reinterpret_cast<PollStatsTask *>(task), rctx);
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
    case Method::kAllocate: {
      MonitorAllocate(mode, reinterpret_cast<AllocateTask *>(task), rctx);
      break;
    }
    case Method::kFree: {
      MonitorFree(mode, reinterpret_cast<FreeTask *>(task), rctx);
      break;
    }
    case Method::kWrite: {
      MonitorWrite(mode, reinterpret_cast<WriteTask *>(task), rctx);
      break;
    }
    case Method::kRead: {
      MonitorRead(mode, reinterpret_cast<ReadTask *>(task), rctx);
      break;
    }
    case Method::kPollStats: {
      MonitorPollStats(mode, reinterpret_cast<PollStatsTask *>(task), rctx);
      break;
    }
  }
}
/** Delete a task */
void Del(u32 method, Task *task) override {
  switch (method) {
    case Method::kCreate: {
      CHI_CLIENT->DelTask<CreateTask>(reinterpret_cast<CreateTask *>(task));
      break;
    }
    case Method::kDestroy: {
      CHI_CLIENT->DelTask<DestroyTask>(reinterpret_cast<DestroyTask *>(task));
      break;
    }
    case Method::kAllocate: {
      CHI_CLIENT->DelTask<AllocateTask>(reinterpret_cast<AllocateTask *>(task));
      break;
    }
    case Method::kFree: {
      CHI_CLIENT->DelTask<FreeTask>(reinterpret_cast<FreeTask *>(task));
      break;
    }
    case Method::kWrite: {
      CHI_CLIENT->DelTask<WriteTask>(reinterpret_cast<WriteTask *>(task));
      break;
    }
    case Method::kRead: {
      CHI_CLIENT->DelTask<ReadTask>(reinterpret_cast<ReadTask *>(task));
      break;
    }
    case Method::kPollStats: {
      CHI_CLIENT->DelTask<PollStatsTask>(reinterpret_cast<PollStatsTask *>(task));
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
    case Method::kAllocate: {
      chi::CALL_COPY_START(
        reinterpret_cast<const AllocateTask*>(orig_task), 
        reinterpret_cast<AllocateTask*>(dup_task), deep);
      break;
    }
    case Method::kFree: {
      chi::CALL_COPY_START(
        reinterpret_cast<const FreeTask*>(orig_task), 
        reinterpret_cast<FreeTask*>(dup_task), deep);
      break;
    }
    case Method::kWrite: {
      chi::CALL_COPY_START(
        reinterpret_cast<const WriteTask*>(orig_task), 
        reinterpret_cast<WriteTask*>(dup_task), deep);
      break;
    }
    case Method::kRead: {
      chi::CALL_COPY_START(
        reinterpret_cast<const ReadTask*>(orig_task), 
        reinterpret_cast<ReadTask*>(dup_task), deep);
      break;
    }
    case Method::kPollStats: {
      chi::CALL_COPY_START(
        reinterpret_cast<const PollStatsTask*>(orig_task), 
        reinterpret_cast<PollStatsTask*>(dup_task), deep);
      break;
    }
  }
}
/** Duplicate a task */
void NewCopyStart(u32 method, const Task *orig_task, LPointer<Task> &dup_task, bool deep) override {
  switch (method) {
    case Method::kCreate: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const CreateTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kDestroy: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const DestroyTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kAllocate: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const AllocateTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kFree: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const FreeTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kWrite: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const WriteTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kRead: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const ReadTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kPollStats: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const PollStatsTask*>(orig_task), dup_task, deep);
      break;
    }
  }
}
/** Serialize a task when initially pushing into remote */
void SaveStart(u32 method, BinaryOutputArchive<true> &ar, Task *task) override {
  switch (method) {
    case Method::kCreate: {
      ar << *reinterpret_cast<CreateTask*>(task);
      break;
    }
    case Method::kDestroy: {
      ar << *reinterpret_cast<DestroyTask*>(task);
      break;
    }
    case Method::kAllocate: {
      ar << *reinterpret_cast<AllocateTask*>(task);
      break;
    }
    case Method::kFree: {
      ar << *reinterpret_cast<FreeTask*>(task);
      break;
    }
    case Method::kWrite: {
      ar << *reinterpret_cast<WriteTask*>(task);
      break;
    }
    case Method::kRead: {
      ar << *reinterpret_cast<ReadTask*>(task);
      break;
    }
    case Method::kPollStats: {
      ar << *reinterpret_cast<PollStatsTask*>(task);
      break;
    }
  }
}
/** Deserialize a task when popping from remote queue */
TaskPointer LoadStart(u32 method, BinaryInputArchive<true> &ar) override {
  TaskPointer task_ptr;
  switch (method) {
    case Method::kCreate: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<CreateTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<CreateTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kDestroy: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<DestroyTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<DestroyTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kAllocate: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<AllocateTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<AllocateTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kFree: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<FreeTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<FreeTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kWrite: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<WriteTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<WriteTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kRead: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<ReadTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<ReadTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kPollStats: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<PollStatsTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<PollStatsTask*>(task_ptr.ptr_);
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
    case Method::kAllocate: {
      ar << *reinterpret_cast<AllocateTask*>(task);
      break;
    }
    case Method::kFree: {
      ar << *reinterpret_cast<FreeTask*>(task);
      break;
    }
    case Method::kWrite: {
      ar << *reinterpret_cast<WriteTask*>(task);
      break;
    }
    case Method::kRead: {
      ar << *reinterpret_cast<ReadTask*>(task);
      break;
    }
    case Method::kPollStats: {
      ar << *reinterpret_cast<PollStatsTask*>(task);
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
    case Method::kAllocate: {
      ar >> *reinterpret_cast<AllocateTask*>(task);
      break;
    }
    case Method::kFree: {
      ar >> *reinterpret_cast<FreeTask*>(task);
      break;
    }
    case Method::kWrite: {
      ar >> *reinterpret_cast<WriteTask*>(task);
      break;
    }
    case Method::kRead: {
      ar >> *reinterpret_cast<ReadTask*>(task);
      break;
    }
    case Method::kPollStats: {
      ar >> *reinterpret_cast<PollStatsTask*>(task);
      break;
    }
  }
}

#endif  // CHI_BDEV_LIB_EXEC_H_