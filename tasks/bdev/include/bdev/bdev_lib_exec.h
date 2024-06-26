#ifndef HRUN_BDEV_LIB_EXEC_H_
#define HRUN_BDEV_LIB_EXEC_H_

/** Execute a task */
void Run(u32 method, Task *task, RunContext &rctx) override {
  switch (method) {
    case Method::kCreate: {
      Create(reinterpret_cast<CreateTask *>(task), rctx);
      break;
    }
    case Method::kDestruct: {
      Destruct(reinterpret_cast<DestructTask *>(task), rctx);
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
  }
}
/** Execute a task */
void Monitor(u32 mode, Task *task, RunContext &rctx) override {
  switch (task->method_) {
    case Method::kCreate: {
      MonitorCreate(mode, reinterpret_cast<CreateTask *>(task), rctx);
      break;
    }
    case Method::kDestruct: {
      MonitorDestruct(mode, reinterpret_cast<DestructTask *>(task), rctx);
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
  }
}
/** Delete a task */
void Del(u32 method, Task *task) override {
  switch (method) {
    case Method::kCreate: {
      CHI_CLIENT->DelTask<CreateTask>(reinterpret_cast<CreateTask *>(task));
      break;
    }
    case Method::kDestruct: {
      CHI_CLIENT->DelTask<DestructTask>(reinterpret_cast<DestructTask *>(task));
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
    case Method::kDestruct: {
      chi::CALL_COPY_START(
        reinterpret_cast<const DestructTask*>(orig_task), 
        reinterpret_cast<DestructTask*>(dup_task), deep);
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
  }
}
/** Duplicate a task */
void NewCopyStart(u32 method, const Task *orig_task, LPointer<Task> &dup_task, bool deep) override {
  switch (method) {
    case Method::kCreate: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const CreateTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kDestruct: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const DestructTask*>(orig_task), dup_task, deep);
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
  }
}
/** Serialize a task when initially pushing into remote */
void SaveStart(u32 method, BinaryOutputArchive<true> &ar, Task *task) override {
  switch (method) {
    case Method::kCreate: {
      ar << *reinterpret_cast<CreateTask*>(task);
      break;
    }
    case Method::kDestruct: {
      ar << *reinterpret_cast<DestructTask*>(task);
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
    case Method::kDestruct: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<DestructTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<DestructTask*>(task_ptr.ptr_);
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
    case Method::kDestruct: {
      ar << *reinterpret_cast<DestructTask*>(task);
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
  }
}
/** Deserialize a task when popping from remote queue */
void LoadEnd(u32 method, BinaryInputArchive<false> &ar, Task *task) override {
  switch (method) {
    case Method::kCreate: {
      ar >> *reinterpret_cast<CreateTask*>(task);
      break;
    }
    case Method::kDestruct: {
      ar >> *reinterpret_cast<DestructTask*>(task);
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
  }
}

#endif  // HRUN_BDEV_METHODS_H_