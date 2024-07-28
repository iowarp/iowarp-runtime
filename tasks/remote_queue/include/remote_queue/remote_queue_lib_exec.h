#ifndef CHI_REMOTE_QUEUE_LIB_EXEC_H_
#define CHI_REMOTE_QUEUE_LIB_EXEC_H_

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
    case Method::kClientPushSubmit: {
      ClientPushSubmit(reinterpret_cast<ClientPushSubmitTask *>(task), rctx);
      break;
    }
    case Method::kClientSubmit: {
      ClientSubmit(reinterpret_cast<ClientSubmitTask *>(task), rctx);
      break;
    }
    case Method::kServerPushComplete: {
      ServerPushComplete(reinterpret_cast<ServerPushCompleteTask *>(task), rctx);
      break;
    }
    case Method::kServerComplete: {
      ServerComplete(reinterpret_cast<ServerCompleteTask *>(task), rctx);
      break;
    }
  }
}
/** Execute a task */
void Monitor(u32 mode, u32 method, Task *task, RunContext &rctx) override {
  switch (method) {
    case Method::kCreate: {
      MonitorCreate(mode, reinterpret_cast<CreateTask *>(task), rctx);
      break;
    }
    case Method::kDestroy: {
      MonitorDestroy(mode, reinterpret_cast<DestroyTask *>(task), rctx);
      break;
    }
    case Method::kClientPushSubmit: {
      MonitorClientPushSubmit(mode, reinterpret_cast<ClientPushSubmitTask *>(task), rctx);
      break;
    }
    case Method::kClientSubmit: {
      MonitorClientSubmit(mode, reinterpret_cast<ClientSubmitTask *>(task), rctx);
      break;
    }
    case Method::kServerPushComplete: {
      MonitorServerPushComplete(mode, reinterpret_cast<ServerPushCompleteTask *>(task), rctx);
      break;
    }
    case Method::kServerComplete: {
      MonitorServerComplete(mode, reinterpret_cast<ServerCompleteTask *>(task), rctx);
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
    case Method::kClientPushSubmit: {
      CHI_CLIENT->DelTask<ClientPushSubmitTask>(reinterpret_cast<ClientPushSubmitTask *>(task));
      break;
    }
    case Method::kClientSubmit: {
      CHI_CLIENT->DelTask<ClientSubmitTask>(reinterpret_cast<ClientSubmitTask *>(task));
      break;
    }
    case Method::kServerPushComplete: {
      CHI_CLIENT->DelTask<ServerPushCompleteTask>(reinterpret_cast<ServerPushCompleteTask *>(task));
      break;
    }
    case Method::kServerComplete: {
      CHI_CLIENT->DelTask<ServerCompleteTask>(reinterpret_cast<ServerCompleteTask *>(task));
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
    case Method::kClientPushSubmit: {
      chi::CALL_COPY_START(
        reinterpret_cast<const ClientPushSubmitTask*>(orig_task), 
        reinterpret_cast<ClientPushSubmitTask*>(dup_task), deep);
      break;
    }
    case Method::kClientSubmit: {
      chi::CALL_COPY_START(
        reinterpret_cast<const ClientSubmitTask*>(orig_task), 
        reinterpret_cast<ClientSubmitTask*>(dup_task), deep);
      break;
    }
    case Method::kServerPushComplete: {
      chi::CALL_COPY_START(
        reinterpret_cast<const ServerPushCompleteTask*>(orig_task), 
        reinterpret_cast<ServerPushCompleteTask*>(dup_task), deep);
      break;
    }
    case Method::kServerComplete: {
      chi::CALL_COPY_START(
        reinterpret_cast<const ServerCompleteTask*>(orig_task), 
        reinterpret_cast<ServerCompleteTask*>(dup_task), deep);
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
    case Method::kClientPushSubmit: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const ClientPushSubmitTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kClientSubmit: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const ClientSubmitTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kServerPushComplete: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const ServerPushCompleteTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kServerComplete: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const ServerCompleteTask*>(orig_task), dup_task, deep);
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
    case Method::kClientPushSubmit: {
      ar << *reinterpret_cast<ClientPushSubmitTask*>(task);
      break;
    }
    case Method::kClientSubmit: {
      ar << *reinterpret_cast<ClientSubmitTask*>(task);
      break;
    }
    case Method::kServerPushComplete: {
      ar << *reinterpret_cast<ServerPushCompleteTask*>(task);
      break;
    }
    case Method::kServerComplete: {
      ar << *reinterpret_cast<ServerCompleteTask*>(task);
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
    case Method::kClientPushSubmit: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<ClientPushSubmitTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<ClientPushSubmitTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kClientSubmit: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<ClientSubmitTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<ClientSubmitTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kServerPushComplete: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<ServerPushCompleteTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<ServerPushCompleteTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kServerComplete: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<ServerCompleteTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<ServerCompleteTask*>(task_ptr.ptr_);
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
    case Method::kClientPushSubmit: {
      ar << *reinterpret_cast<ClientPushSubmitTask*>(task);
      break;
    }
    case Method::kClientSubmit: {
      ar << *reinterpret_cast<ClientSubmitTask*>(task);
      break;
    }
    case Method::kServerPushComplete: {
      ar << *reinterpret_cast<ServerPushCompleteTask*>(task);
      break;
    }
    case Method::kServerComplete: {
      ar << *reinterpret_cast<ServerCompleteTask*>(task);
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
    case Method::kClientPushSubmit: {
      ar >> *reinterpret_cast<ClientPushSubmitTask*>(task);
      break;
    }
    case Method::kClientSubmit: {
      ar >> *reinterpret_cast<ClientSubmitTask*>(task);
      break;
    }
    case Method::kServerPushComplete: {
      ar >> *reinterpret_cast<ServerPushCompleteTask*>(task);
      break;
    }
    case Method::kServerComplete: {
      ar >> *reinterpret_cast<ServerCompleteTask*>(task);
      break;
    }
  }
}

#endif  // CHI_REMOTE_QUEUE_METHODS_H_