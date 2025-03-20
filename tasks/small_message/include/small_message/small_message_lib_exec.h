#ifndef CHI_SMALL_MESSAGE_LIB_EXEC_H_
#define CHI_SMALL_MESSAGE_LIB_EXEC_H_

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
    case Method::kUpgrade: {
      Upgrade(reinterpret_cast<UpgradeTask *>(task), rctx);
      break;
    }
    case Method::kMd: {
      Md(reinterpret_cast<MdTask *>(task), rctx);
      break;
    }
    case Method::kIo: {
      Io(reinterpret_cast<IoTask *>(task), rctx);
      break;
    }
  }
}
/** Execute a task */
void Monitor(MonitorModeId mode, MethodId method, Task *task,
             RunContext &rctx) override {
  switch (method) {
    case Method::kCreate: {
      MonitorCreate(mode, reinterpret_cast<CreateTask *>(task), rctx);
      break;
    }
    case Method::kDestroy: {
      MonitorDestroy(mode, reinterpret_cast<DestroyTask *>(task), rctx);
      break;
    }
    case Method::kUpgrade: {
      MonitorUpgrade(mode, reinterpret_cast<UpgradeTask *>(task), rctx);
      break;
    }
    case Method::kMd: {
      MonitorMd(mode, reinterpret_cast<MdTask *>(task), rctx);
      break;
    }
    case Method::kIo: {
      MonitorIo(mode, reinterpret_cast<IoTask *>(task), rctx);
      break;
    }
  }
}
/** Delete a task */
void Del(const hipc::MemContext &mctx, u32 method, Task *task) override {
  switch (method) {
    case Method::kCreate: {
      CHI_CLIENT->DelTask<CreateTask>(mctx,
                                      reinterpret_cast<CreateTask *>(task));
      break;
    }
    case Method::kDestroy: {
      CHI_CLIENT->DelTask<DestroyTask>(mctx,
                                       reinterpret_cast<DestroyTask *>(task));
      break;
    }
    case Method::kUpgrade: {
      CHI_CLIENT->DelTask<UpgradeTask>(mctx,
                                       reinterpret_cast<UpgradeTask *>(task));
      break;
    }
    case Method::kMd: {
      CHI_CLIENT->DelTask<MdTask>(mctx, reinterpret_cast<MdTask *>(task));
      break;
    }
    case Method::kIo: {
      CHI_CLIENT->DelTask<IoTask>(mctx, reinterpret_cast<IoTask *>(task));
      break;
    }
  }
}
/** Duplicate a task */
void CopyStart(u32 method, const Task *orig_task, Task *dup_task,
               bool deep) override {
  switch (method) {
    case Method::kCreate: {
      chi::CALL_COPY_START(reinterpret_cast<const CreateTask *>(orig_task),
                           reinterpret_cast<CreateTask *>(dup_task), deep);
      break;
    }
    case Method::kDestroy: {
      chi::CALL_COPY_START(reinterpret_cast<const DestroyTask *>(orig_task),
                           reinterpret_cast<DestroyTask *>(dup_task), deep);
      break;
    }
    case Method::kUpgrade: {
      chi::CALL_COPY_START(reinterpret_cast<const UpgradeTask *>(orig_task),
                           reinterpret_cast<UpgradeTask *>(dup_task), deep);
      break;
    }
    case Method::kMd: {
      chi::CALL_COPY_START(reinterpret_cast<const MdTask *>(orig_task),
                           reinterpret_cast<MdTask *>(dup_task), deep);
      break;
    }
    case Method::kIo: {
      chi::CALL_COPY_START(reinterpret_cast<const IoTask *>(orig_task),
                           reinterpret_cast<IoTask *>(dup_task), deep);
      break;
    }
  }
}
/** Duplicate a task */
void NewCopyStart(u32 method, const Task *orig_task, FullPtr<Task> &dup_task,
                  bool deep) override {
  switch (method) {
    case Method::kCreate: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const CreateTask *>(orig_task),
                               dup_task, deep);
      break;
    }
    case Method::kDestroy: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const DestroyTask *>(orig_task),
                               dup_task, deep);
      break;
    }
    case Method::kUpgrade: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const UpgradeTask *>(orig_task),
                               dup_task, deep);
      break;
    }
    case Method::kMd: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const MdTask *>(orig_task),
                               dup_task, deep);
      break;
    }
    case Method::kIo: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const IoTask *>(orig_task),
                               dup_task, deep);
      break;
    }
  }
}
/** Serialize a task when initially pushing into remote */
void SaveStart(u32 method, BinaryOutputArchive<true> &ar, Task *task) override {
  switch (method) {
    case Method::kCreate: {
      ar << *reinterpret_cast<CreateTask *>(task);
      break;
    }
    case Method::kDestroy: {
      ar << *reinterpret_cast<DestroyTask *>(task);
      break;
    }
    case Method::kUpgrade: {
      ar << *reinterpret_cast<UpgradeTask *>(task);
      break;
    }
    case Method::kMd: {
      ar << *reinterpret_cast<MdTask *>(task);
      break;
    }
    case Method::kIo: {
      ar << *reinterpret_cast<IoTask *>(task);
      break;
    }
  }
}
/** Deserialize a task when popping from remote queue */
TaskPointer LoadStart(u32 method, BinaryInputArchive<true> &ar) override {
  TaskPointer task_ptr;
  switch (method) {
    case Method::kCreate: {
      task_ptr.ptr_ =
          CHI_CLIENT->NewEmptyTask<CreateTask>(HSHM_MCTX, task_ptr.shm_);
      ar >> *reinterpret_cast<CreateTask *>(task_ptr.ptr_);
      break;
    }
    case Method::kDestroy: {
      task_ptr.ptr_ =
          CHI_CLIENT->NewEmptyTask<DestroyTask>(HSHM_MCTX, task_ptr.shm_);
      ar >> *reinterpret_cast<DestroyTask *>(task_ptr.ptr_);
      break;
    }
    case Method::kUpgrade: {
      task_ptr.ptr_ =
          CHI_CLIENT->NewEmptyTask<UpgradeTask>(HSHM_MCTX, task_ptr.shm_);
      ar >> *reinterpret_cast<UpgradeTask *>(task_ptr.ptr_);
      break;
    }
    case Method::kMd: {
      task_ptr.ptr_ =
          CHI_CLIENT->NewEmptyTask<MdTask>(HSHM_MCTX, task_ptr.shm_);
      ar >> *reinterpret_cast<MdTask *>(task_ptr.ptr_);
      break;
    }
    case Method::kIo: {
      task_ptr.ptr_ =
          CHI_CLIENT->NewEmptyTask<IoTask>(HSHM_MCTX, task_ptr.shm_);
      ar >> *reinterpret_cast<IoTask *>(task_ptr.ptr_);
      break;
    }
  }
  return task_ptr;
}
/** Serialize a task when returning from remote queue */
void SaveEnd(u32 method, BinaryOutputArchive<false> &ar, Task *task) override {
  switch (method) {
    case Method::kCreate: {
      ar << *reinterpret_cast<CreateTask *>(task);
      break;
    }
    case Method::kDestroy: {
      ar << *reinterpret_cast<DestroyTask *>(task);
      break;
    }
    case Method::kUpgrade: {
      ar << *reinterpret_cast<UpgradeTask *>(task);
      break;
    }
    case Method::kMd: {
      ar << *reinterpret_cast<MdTask *>(task);
      break;
    }
    case Method::kIo: {
      ar << *reinterpret_cast<IoTask *>(task);
      break;
    }
  }
}
/** Deserialize a task when popping from remote queue */
void LoadEnd(u32 method, BinaryInputArchive<false> &ar, Task *task) override {
  switch (method) {
    case Method::kCreate: {
      ar >> *reinterpret_cast<CreateTask *>(task);
      break;
    }
    case Method::kDestroy: {
      ar >> *reinterpret_cast<DestroyTask *>(task);
      break;
    }
    case Method::kUpgrade: {
      ar >> *reinterpret_cast<UpgradeTask *>(task);
      break;
    }
    case Method::kMd: {
      ar >> *reinterpret_cast<MdTask *>(task);
      break;
    }
    case Method::kIo: {
      ar >> *reinterpret_cast<IoTask *>(task);
      break;
    }
  }
}

#endif  // CHI_SMALL_MESSAGE_LIB_EXEC_H_