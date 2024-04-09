#ifndef HRUN_REMOTE_QUEUE_LIB_EXEC_H_
#define HRUN_REMOTE_QUEUE_LIB_EXEC_H_

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
      HRUN_CLIENT->DelTask<CreateTask>(reinterpret_cast<CreateTask *>(task));
      break;
    }
    case Method::kDestruct: {
      HRUN_CLIENT->DelTask<DestructTask>(reinterpret_cast<DestructTask *>(task));
      break;
    }
    case Method::kClientPushSubmit: {
      HRUN_CLIENT->DelTask<ClientPushSubmitTask>(reinterpret_cast<ClientPushSubmitTask *>(task));
      break;
    }
    case Method::kClientSubmit: {
      HRUN_CLIENT->DelTask<ClientSubmitTask>(reinterpret_cast<ClientSubmitTask *>(task));
      break;
    }
    case Method::kServerPushComplete: {
      HRUN_CLIENT->DelTask<ServerPushCompleteTask>(reinterpret_cast<ServerPushCompleteTask *>(task));
      break;
    }
    case Method::kServerComplete: {
      HRUN_CLIENT->DelTask<ServerCompleteTask>(reinterpret_cast<ServerCompleteTask *>(task));
      break;
    }
  }
}
/** Duplicate a task */
void CopyStart(u32 method, Task *orig_task, LPointer<Task> &dup_task) override {
  switch (method) {
    case Method::kCreate: {
      chm::CALL_COPY_START(reinterpret_cast<CreateTask*>(orig_task), dup_task);
      break;
    }
    case Method::kDestruct: {
      chm::CALL_COPY_START(reinterpret_cast<DestructTask*>(orig_task), dup_task);
      break;
    }
    case Method::kClientPushSubmit: {
      chm::CALL_COPY_START(reinterpret_cast<ClientPushSubmitTask*>(orig_task), dup_task);
      break;
    }
    case Method::kClientSubmit: {
      chm::CALL_COPY_START(reinterpret_cast<ClientSubmitTask*>(orig_task), dup_task);
      break;
    }
    case Method::kServerPushComplete: {
      chm::CALL_COPY_START(reinterpret_cast<ServerPushCompleteTask*>(orig_task), dup_task);
      break;
    }
    case Method::kServerComplete: {
      chm::CALL_COPY_START(reinterpret_cast<ServerCompleteTask*>(orig_task), dup_task);
      break;
    }
  }
}
/** Register the duplicate output with the origin task */
void CopyEnd(u32 method, Task *orig_task, Task *dup_task) override {
  switch (method) {
    case Method::kCreate: {
      chm::CALL_COPY_END(reinterpret_cast<CreateTask*>(orig_task), reinterpret_cast<CreateTask*>(dup_task));
      break;
    }
    case Method::kDestruct: {
      chm::CALL_COPY_END(reinterpret_cast<DestructTask*>(orig_task), reinterpret_cast<DestructTask*>(dup_task));
      break;
    }
    case Method::kClientPushSubmit: {
      chm::CALL_COPY_END(reinterpret_cast<ClientPushSubmitTask*>(orig_task), reinterpret_cast<ClientPushSubmitTask*>(dup_task));
      break;
    }
    case Method::kClientSubmit: {
      chm::CALL_COPY_END(reinterpret_cast<ClientSubmitTask*>(orig_task), reinterpret_cast<ClientSubmitTask*>(dup_task));
      break;
    }
    case Method::kServerPushComplete: {
      chm::CALL_COPY_END(reinterpret_cast<ServerPushCompleteTask*>(orig_task), reinterpret_cast<ServerPushCompleteTask*>(dup_task));
      break;
    }
    case Method::kServerComplete: {
      chm::CALL_COPY_END(reinterpret_cast<ServerCompleteTask*>(orig_task), reinterpret_cast<ServerCompleteTask*>(dup_task));
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
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<CreateTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<CreateTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kDestruct: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<DestructTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<DestructTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kClientPushSubmit: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<ClientPushSubmitTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<ClientPushSubmitTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kClientSubmit: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<ClientSubmitTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<ClientSubmitTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kServerPushComplete: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<ServerPushCompleteTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<ServerPushCompleteTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kServerComplete: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<ServerCompleteTask>(task_ptr.shm_);
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
    case Method::kDestruct: {
      ar << *reinterpret_cast<DestructTask*>(task);
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
    case Method::kDestruct: {
      ar >> *reinterpret_cast<DestructTask*>(task);
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
/** Deserialize a task when popping from remote queue */
TaskPointer LoadReplicaEnd(u32 method, BinaryInputArchive<false> &ar, Task *task) override {
  TaskPointer task_ptr;
  switch (method) {
    case Method::kCreate: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<CreateTask>(task_ptr.shm_);
      task_ptr.ptr_->task_dup(*task);
      ar >> *reinterpret_cast<CreateTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kDestruct: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<DestructTask>(task_ptr.shm_);
      task_ptr.ptr_->task_dup(*task);
      ar >> *reinterpret_cast<DestructTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kClientPushSubmit: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<ClientPushSubmitTask>(task_ptr.shm_);
      task_ptr.ptr_->task_dup(*task);
      ar >> *reinterpret_cast<ClientPushSubmitTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kClientSubmit: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<ClientSubmitTask>(task_ptr.shm_);
      task_ptr.ptr_->task_dup(*task);
      ar >> *reinterpret_cast<ClientSubmitTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kServerPushComplete: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<ServerPushCompleteTask>(task_ptr.shm_);
      task_ptr.ptr_->task_dup(*task);
      ar >> *reinterpret_cast<ServerPushCompleteTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kServerComplete: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<ServerCompleteTask>(task_ptr.shm_);
      task_ptr.ptr_->task_dup(*task);
      ar >> *reinterpret_cast<ServerCompleteTask*>(task_ptr.ptr_);
      break;
    }
  }
  return task_ptr;
}

#endif  // HRUN_REMOTE_QUEUE_METHODS_H_