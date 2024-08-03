#ifndef CHI_CHIFS_LIB_EXEC_H_
#define CHI_CHIFS_LIB_EXEC_H_

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
    case Method::kOpenFile: {
      OpenFile(reinterpret_cast<OpenFileTask *>(task), rctx);
      break;
    }
    case Method::kReadFile: {
      ReadFile(reinterpret_cast<ReadFileTask *>(task), rctx);
      break;
    }
    case Method::kWriteFile: {
      WriteFile(reinterpret_cast<WriteFileTask *>(task), rctx);
      break;
    }
    case Method::kAppendFile: {
      AppendFile(reinterpret_cast<AppendFileTask *>(task), rctx);
      break;
    }
    case Method::kDestroyFile: {
      DestroyFile(reinterpret_cast<DestroyFileTask *>(task), rctx);
      break;
    }
    case Method::kStatFile: {
      StatFile(reinterpret_cast<StatFileTask *>(task), rctx);
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
    case Method::kOpenFile: {
      MonitorOpenFile(mode, reinterpret_cast<OpenFileTask *>(task), rctx);
      break;
    }
    case Method::kReadFile: {
      MonitorReadFile(mode, reinterpret_cast<ReadFileTask *>(task), rctx);
      break;
    }
    case Method::kWriteFile: {
      MonitorWriteFile(mode, reinterpret_cast<WriteFileTask *>(task), rctx);
      break;
    }
    case Method::kAppendFile: {
      MonitorAppendFile(mode, reinterpret_cast<AppendFileTask *>(task), rctx);
      break;
    }
    case Method::kDestroyFile: {
      MonitorDestroyFile(mode, reinterpret_cast<DestroyFileTask *>(task), rctx);
      break;
    }
    case Method::kStatFile: {
      MonitorStatFile(mode, reinterpret_cast<StatFileTask *>(task), rctx);
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
    case Method::kOpenFile: {
      CHI_CLIENT->DelTask<OpenFileTask>(reinterpret_cast<OpenFileTask *>(task));
      break;
    }
    case Method::kReadFile: {
      CHI_CLIENT->DelTask<ReadFileTask>(reinterpret_cast<ReadFileTask *>(task));
      break;
    }
    case Method::kWriteFile: {
      CHI_CLIENT->DelTask<WriteFileTask>(reinterpret_cast<WriteFileTask *>(task));
      break;
    }
    case Method::kAppendFile: {
      CHI_CLIENT->DelTask<AppendFileTask>(reinterpret_cast<AppendFileTask *>(task));
      break;
    }
    case Method::kDestroyFile: {
      CHI_CLIENT->DelTask<DestroyFileTask>(reinterpret_cast<DestroyFileTask *>(task));
      break;
    }
    case Method::kStatFile: {
      CHI_CLIENT->DelTask<StatFileTask>(reinterpret_cast<StatFileTask *>(task));
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
    case Method::kOpenFile: {
      chi::CALL_COPY_START(
        reinterpret_cast<const OpenFileTask*>(orig_task), 
        reinterpret_cast<OpenFileTask*>(dup_task), deep);
      break;
    }
    case Method::kReadFile: {
      chi::CALL_COPY_START(
        reinterpret_cast<const ReadFileTask*>(orig_task), 
        reinterpret_cast<ReadFileTask*>(dup_task), deep);
      break;
    }
    case Method::kWriteFile: {
      chi::CALL_COPY_START(
        reinterpret_cast<const WriteFileTask*>(orig_task), 
        reinterpret_cast<WriteFileTask*>(dup_task), deep);
      break;
    }
    case Method::kAppendFile: {
      chi::CALL_COPY_START(
        reinterpret_cast<const AppendFileTask*>(orig_task), 
        reinterpret_cast<AppendFileTask*>(dup_task), deep);
      break;
    }
    case Method::kDestroyFile: {
      chi::CALL_COPY_START(
        reinterpret_cast<const DestroyFileTask*>(orig_task), 
        reinterpret_cast<DestroyFileTask*>(dup_task), deep);
      break;
    }
    case Method::kStatFile: {
      chi::CALL_COPY_START(
        reinterpret_cast<const StatFileTask*>(orig_task), 
        reinterpret_cast<StatFileTask*>(dup_task), deep);
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
    case Method::kOpenFile: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const OpenFileTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kReadFile: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const ReadFileTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kWriteFile: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const WriteFileTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kAppendFile: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const AppendFileTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kDestroyFile: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const DestroyFileTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kStatFile: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const StatFileTask*>(orig_task), dup_task, deep);
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
    case Method::kOpenFile: {
      ar << *reinterpret_cast<OpenFileTask*>(task);
      break;
    }
    case Method::kReadFile: {
      ar << *reinterpret_cast<ReadFileTask*>(task);
      break;
    }
    case Method::kWriteFile: {
      ar << *reinterpret_cast<WriteFileTask*>(task);
      break;
    }
    case Method::kAppendFile: {
      ar << *reinterpret_cast<AppendFileTask*>(task);
      break;
    }
    case Method::kDestroyFile: {
      ar << *reinterpret_cast<DestroyFileTask*>(task);
      break;
    }
    case Method::kStatFile: {
      ar << *reinterpret_cast<StatFileTask*>(task);
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
    case Method::kOpenFile: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<OpenFileTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<OpenFileTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kReadFile: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<ReadFileTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<ReadFileTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kWriteFile: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<WriteFileTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<WriteFileTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kAppendFile: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<AppendFileTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<AppendFileTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kDestroyFile: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<DestroyFileTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<DestroyFileTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kStatFile: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<StatFileTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<StatFileTask*>(task_ptr.ptr_);
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
    case Method::kOpenFile: {
      ar << *reinterpret_cast<OpenFileTask*>(task);
      break;
    }
    case Method::kReadFile: {
      ar << *reinterpret_cast<ReadFileTask*>(task);
      break;
    }
    case Method::kWriteFile: {
      ar << *reinterpret_cast<WriteFileTask*>(task);
      break;
    }
    case Method::kAppendFile: {
      ar << *reinterpret_cast<AppendFileTask*>(task);
      break;
    }
    case Method::kDestroyFile: {
      ar << *reinterpret_cast<DestroyFileTask*>(task);
      break;
    }
    case Method::kStatFile: {
      ar << *reinterpret_cast<StatFileTask*>(task);
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
    case Method::kOpenFile: {
      ar >> *reinterpret_cast<OpenFileTask*>(task);
      break;
    }
    case Method::kReadFile: {
      ar >> *reinterpret_cast<ReadFileTask*>(task);
      break;
    }
    case Method::kWriteFile: {
      ar >> *reinterpret_cast<WriteFileTask*>(task);
      break;
    }
    case Method::kAppendFile: {
      ar >> *reinterpret_cast<AppendFileTask*>(task);
      break;
    }
    case Method::kDestroyFile: {
      ar >> *reinterpret_cast<DestroyFileTask*>(task);
      break;
    }
    case Method::kStatFile: {
      ar >> *reinterpret_cast<StatFileTask*>(task);
      break;
    }
  }
}

#endif  // CHI_CHIFS_METHODS_H_