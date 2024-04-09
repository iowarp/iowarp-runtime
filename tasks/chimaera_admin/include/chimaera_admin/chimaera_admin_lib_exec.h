#ifndef HRUN_CHIMAERA_ADMIN_LIB_EXEC_H_
#define HRUN_CHIMAERA_ADMIN_LIB_EXEC_H_

/** Execute a task */
void Run(u32 method, Task *task, RunContext &rctx) override {
  switch (method) {
    case Method::kCreateTaskState: {
      CreateTaskState(reinterpret_cast<CreateTaskStateTask *>(task), rctx);
      break;
    }
    case Method::kDestroyTaskState: {
      DestroyTaskState(reinterpret_cast<DestroyTaskStateTask *>(task), rctx);
      break;
    }
    case Method::kRegisterTaskLib: {
      RegisterTaskLib(reinterpret_cast<RegisterTaskLibTask *>(task), rctx);
      break;
    }
    case Method::kDestroyTaskLib: {
      DestroyTaskLib(reinterpret_cast<DestroyTaskLibTask *>(task), rctx);
      break;
    }
    case Method::kGetOrCreateTaskStateId: {
      GetOrCreateTaskStateId(reinterpret_cast<GetOrCreateTaskStateIdTask *>(task), rctx);
      break;
    }
    case Method::kGetTaskStateId: {
      GetTaskStateId(reinterpret_cast<GetTaskStateIdTask *>(task), rctx);
      break;
    }
    case Method::kStopRuntime: {
      StopRuntime(reinterpret_cast<StopRuntimeTask *>(task), rctx);
      break;
    }
    case Method::kSetWorkOrchQueuePolicy: {
      SetWorkOrchQueuePolicy(reinterpret_cast<SetWorkOrchQueuePolicyTask *>(task), rctx);
      break;
    }
    case Method::kSetWorkOrchProcPolicy: {
      SetWorkOrchProcPolicy(reinterpret_cast<SetWorkOrchProcPolicyTask *>(task), rctx);
      break;
    }
    case Method::kFlush: {
      Flush(reinterpret_cast<FlushTask *>(task), rctx);
      break;
    }
    case Method::kDomainSize: {
      DomainSize(reinterpret_cast<DomainSizeTask *>(task), rctx);
      break;
    }
  }
}
/** Execute a task */
void Monitor(u32 mode, Task *task, RunContext &rctx) override {
  switch (task->method_) {
    case Method::kCreateTaskState: {
      MonitorCreateTaskState(mode, reinterpret_cast<CreateTaskStateTask *>(task), rctx);
      break;
    }
    case Method::kDestroyTaskState: {
      MonitorDestroyTaskState(mode, reinterpret_cast<DestroyTaskStateTask *>(task), rctx);
      break;
    }
    case Method::kRegisterTaskLib: {
      MonitorRegisterTaskLib(mode, reinterpret_cast<RegisterTaskLibTask *>(task), rctx);
      break;
    }
    case Method::kDestroyTaskLib: {
      MonitorDestroyTaskLib(mode, reinterpret_cast<DestroyTaskLibTask *>(task), rctx);
      break;
    }
    case Method::kGetOrCreateTaskStateId: {
      MonitorGetOrCreateTaskStateId(mode, reinterpret_cast<GetOrCreateTaskStateIdTask *>(task), rctx);
      break;
    }
    case Method::kGetTaskStateId: {
      MonitorGetTaskStateId(mode, reinterpret_cast<GetTaskStateIdTask *>(task), rctx);
      break;
    }
    case Method::kStopRuntime: {
      MonitorStopRuntime(mode, reinterpret_cast<StopRuntimeTask *>(task), rctx);
      break;
    }
    case Method::kSetWorkOrchQueuePolicy: {
      MonitorSetWorkOrchQueuePolicy(mode, reinterpret_cast<SetWorkOrchQueuePolicyTask *>(task), rctx);
      break;
    }
    case Method::kSetWorkOrchProcPolicy: {
      MonitorSetWorkOrchProcPolicy(mode, reinterpret_cast<SetWorkOrchProcPolicyTask *>(task), rctx);
      break;
    }
    case Method::kFlush: {
      MonitorFlush(mode, reinterpret_cast<FlushTask *>(task), rctx);
      break;
    }
    case Method::kDomainSize: {
      MonitorDomainSize(mode, reinterpret_cast<DomainSizeTask *>(task), rctx);
      break;
    }
  }
}
/** Delete a task */
void Del(u32 method, Task *task) override {
  switch (method) {
    case Method::kCreateTaskState: {
      HRUN_CLIENT->DelTask<CreateTaskStateTask>(reinterpret_cast<CreateTaskStateTask *>(task));
      break;
    }
    case Method::kDestroyTaskState: {
      HRUN_CLIENT->DelTask<DestroyTaskStateTask>(reinterpret_cast<DestroyTaskStateTask *>(task));
      break;
    }
    case Method::kRegisterTaskLib: {
      HRUN_CLIENT->DelTask<RegisterTaskLibTask>(reinterpret_cast<RegisterTaskLibTask *>(task));
      break;
    }
    case Method::kDestroyTaskLib: {
      HRUN_CLIENT->DelTask<DestroyTaskLibTask>(reinterpret_cast<DestroyTaskLibTask *>(task));
      break;
    }
    case Method::kGetOrCreateTaskStateId: {
      HRUN_CLIENT->DelTask<GetOrCreateTaskStateIdTask>(reinterpret_cast<GetOrCreateTaskStateIdTask *>(task));
      break;
    }
    case Method::kGetTaskStateId: {
      HRUN_CLIENT->DelTask<GetTaskStateIdTask>(reinterpret_cast<GetTaskStateIdTask *>(task));
      break;
    }
    case Method::kStopRuntime: {
      HRUN_CLIENT->DelTask<StopRuntimeTask>(reinterpret_cast<StopRuntimeTask *>(task));
      break;
    }
    case Method::kSetWorkOrchQueuePolicy: {
      HRUN_CLIENT->DelTask<SetWorkOrchQueuePolicyTask>(reinterpret_cast<SetWorkOrchQueuePolicyTask *>(task));
      break;
    }
    case Method::kSetWorkOrchProcPolicy: {
      HRUN_CLIENT->DelTask<SetWorkOrchProcPolicyTask>(reinterpret_cast<SetWorkOrchProcPolicyTask *>(task));
      break;
    }
    case Method::kFlush: {
      HRUN_CLIENT->DelTask<FlushTask>(reinterpret_cast<FlushTask *>(task));
      break;
    }
    case Method::kDomainSize: {
      HRUN_CLIENT->DelTask<DomainSizeTask>(reinterpret_cast<DomainSizeTask *>(task));
      break;
    }
  }
}
/** Duplicate a task */
void CopyStart(u32 method, Task *orig_task, LPointer<Task> &dup_task) override {
  switch (method) {
    case Method::kCreateTaskState: {
      chm::CALL_COPY_START(reinterpret_cast<CreateTaskStateTask*>(orig_task), dup_task);
      break;
    }
    case Method::kDestroyTaskState: {
      chm::CALL_COPY_START(reinterpret_cast<DestroyTaskStateTask*>(orig_task), dup_task);
      break;
    }
    case Method::kRegisterTaskLib: {
      chm::CALL_COPY_START(reinterpret_cast<RegisterTaskLibTask*>(orig_task), dup_task);
      break;
    }
    case Method::kDestroyTaskLib: {
      chm::CALL_COPY_START(reinterpret_cast<DestroyTaskLibTask*>(orig_task), dup_task);
      break;
    }
    case Method::kGetOrCreateTaskStateId: {
      chm::CALL_COPY_START(reinterpret_cast<GetOrCreateTaskStateIdTask*>(orig_task), dup_task);
      break;
    }
    case Method::kGetTaskStateId: {
      chm::CALL_COPY_START(reinterpret_cast<GetTaskStateIdTask*>(orig_task), dup_task);
      break;
    }
    case Method::kStopRuntime: {
      chm::CALL_COPY_START(reinterpret_cast<StopRuntimeTask*>(orig_task), dup_task);
      break;
    }
    case Method::kSetWorkOrchQueuePolicy: {
      chm::CALL_COPY_START(reinterpret_cast<SetWorkOrchQueuePolicyTask*>(orig_task), dup_task);
      break;
    }
    case Method::kSetWorkOrchProcPolicy: {
      chm::CALL_COPY_START(reinterpret_cast<SetWorkOrchProcPolicyTask*>(orig_task), dup_task);
      break;
    }
    case Method::kFlush: {
      chm::CALL_COPY_START(reinterpret_cast<FlushTask*>(orig_task), dup_task);
      break;
    }
    case Method::kDomainSize: {
      chm::CALL_COPY_START(reinterpret_cast<DomainSizeTask*>(orig_task), dup_task);
      break;
    }
  }
}
/** Register the duplicate output with the origin task */
void CopyEnd(u32 method, Task *orig_task, Task *dup_task) override {
  switch (method) {
    case Method::kCreateTaskState: {
      chm::CALL_COPY_END(reinterpret_cast<CreateTaskStateTask*>(orig_task), reinterpret_cast<CreateTaskStateTask*>(dup_task));
      break;
    }
    case Method::kDestroyTaskState: {
      chm::CALL_COPY_END(reinterpret_cast<DestroyTaskStateTask*>(orig_task), reinterpret_cast<DestroyTaskStateTask*>(dup_task));
      break;
    }
    case Method::kRegisterTaskLib: {
      chm::CALL_COPY_END(reinterpret_cast<RegisterTaskLibTask*>(orig_task), reinterpret_cast<RegisterTaskLibTask*>(dup_task));
      break;
    }
    case Method::kDestroyTaskLib: {
      chm::CALL_COPY_END(reinterpret_cast<DestroyTaskLibTask*>(orig_task), reinterpret_cast<DestroyTaskLibTask*>(dup_task));
      break;
    }
    case Method::kGetOrCreateTaskStateId: {
      chm::CALL_COPY_END(reinterpret_cast<GetOrCreateTaskStateIdTask*>(orig_task), reinterpret_cast<GetOrCreateTaskStateIdTask*>(dup_task));
      break;
    }
    case Method::kGetTaskStateId: {
      chm::CALL_COPY_END(reinterpret_cast<GetTaskStateIdTask*>(orig_task), reinterpret_cast<GetTaskStateIdTask*>(dup_task));
      break;
    }
    case Method::kStopRuntime: {
      chm::CALL_COPY_END(reinterpret_cast<StopRuntimeTask*>(orig_task), reinterpret_cast<StopRuntimeTask*>(dup_task));
      break;
    }
    case Method::kSetWorkOrchQueuePolicy: {
      chm::CALL_COPY_END(reinterpret_cast<SetWorkOrchQueuePolicyTask*>(orig_task), reinterpret_cast<SetWorkOrchQueuePolicyTask*>(dup_task));
      break;
    }
    case Method::kSetWorkOrchProcPolicy: {
      chm::CALL_COPY_END(reinterpret_cast<SetWorkOrchProcPolicyTask*>(orig_task), reinterpret_cast<SetWorkOrchProcPolicyTask*>(dup_task));
      break;
    }
    case Method::kFlush: {
      chm::CALL_COPY_END(reinterpret_cast<FlushTask*>(orig_task), reinterpret_cast<FlushTask*>(dup_task));
      break;
    }
    case Method::kDomainSize: {
      chm::CALL_COPY_END(reinterpret_cast<DomainSizeTask*>(orig_task), reinterpret_cast<DomainSizeTask*>(dup_task));
      break;
    }
  }
}
/** Serialize a task when initially pushing into remote */
void SaveStart(u32 method, BinaryOutputArchive<true> &ar, Task *task) override {
  switch (method) {
    case Method::kCreateTaskState: {
      ar << *reinterpret_cast<CreateTaskStateTask*>(task);
      break;
    }
    case Method::kDestroyTaskState: {
      ar << *reinterpret_cast<DestroyTaskStateTask*>(task);
      break;
    }
    case Method::kRegisterTaskLib: {
      ar << *reinterpret_cast<RegisterTaskLibTask*>(task);
      break;
    }
    case Method::kDestroyTaskLib: {
      ar << *reinterpret_cast<DestroyTaskLibTask*>(task);
      break;
    }
    case Method::kGetOrCreateTaskStateId: {
      ar << *reinterpret_cast<GetOrCreateTaskStateIdTask*>(task);
      break;
    }
    case Method::kGetTaskStateId: {
      ar << *reinterpret_cast<GetTaskStateIdTask*>(task);
      break;
    }
    case Method::kStopRuntime: {
      ar << *reinterpret_cast<StopRuntimeTask*>(task);
      break;
    }
    case Method::kSetWorkOrchQueuePolicy: {
      ar << *reinterpret_cast<SetWorkOrchQueuePolicyTask*>(task);
      break;
    }
    case Method::kSetWorkOrchProcPolicy: {
      ar << *reinterpret_cast<SetWorkOrchProcPolicyTask*>(task);
      break;
    }
    case Method::kFlush: {
      ar << *reinterpret_cast<FlushTask*>(task);
      break;
    }
    case Method::kDomainSize: {
      ar << *reinterpret_cast<DomainSizeTask*>(task);
      break;
    }
  }
}
/** Deserialize a task when popping from remote queue */
TaskPointer LoadStart(u32 method, BinaryInputArchive<true> &ar) override {
  TaskPointer task_ptr;
  switch (method) {
    case Method::kCreateTaskState: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<CreateTaskStateTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<CreateTaskStateTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kDestroyTaskState: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<DestroyTaskStateTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<DestroyTaskStateTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kRegisterTaskLib: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<RegisterTaskLibTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<RegisterTaskLibTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kDestroyTaskLib: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<DestroyTaskLibTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<DestroyTaskLibTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kGetOrCreateTaskStateId: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<GetOrCreateTaskStateIdTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<GetOrCreateTaskStateIdTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kGetTaskStateId: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<GetTaskStateIdTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<GetTaskStateIdTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kStopRuntime: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<StopRuntimeTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<StopRuntimeTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kSetWorkOrchQueuePolicy: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<SetWorkOrchQueuePolicyTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<SetWorkOrchQueuePolicyTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kSetWorkOrchProcPolicy: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<SetWorkOrchProcPolicyTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<SetWorkOrchProcPolicyTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kFlush: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<FlushTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<FlushTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kDomainSize: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<DomainSizeTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<DomainSizeTask*>(task_ptr.ptr_);
      break;
    }
  }
  return task_ptr;
}
/** Serialize a task when returning from remote queue */
void SaveEnd(u32 method, BinaryOutputArchive<false> &ar, Task *task) override {
  switch (method) {
    case Method::kCreateTaskState: {
      ar << *reinterpret_cast<CreateTaskStateTask*>(task);
      break;
    }
    case Method::kDestroyTaskState: {
      ar << *reinterpret_cast<DestroyTaskStateTask*>(task);
      break;
    }
    case Method::kRegisterTaskLib: {
      ar << *reinterpret_cast<RegisterTaskLibTask*>(task);
      break;
    }
    case Method::kDestroyTaskLib: {
      ar << *reinterpret_cast<DestroyTaskLibTask*>(task);
      break;
    }
    case Method::kGetOrCreateTaskStateId: {
      ar << *reinterpret_cast<GetOrCreateTaskStateIdTask*>(task);
      break;
    }
    case Method::kGetTaskStateId: {
      ar << *reinterpret_cast<GetTaskStateIdTask*>(task);
      break;
    }
    case Method::kStopRuntime: {
      ar << *reinterpret_cast<StopRuntimeTask*>(task);
      break;
    }
    case Method::kSetWorkOrchQueuePolicy: {
      ar << *reinterpret_cast<SetWorkOrchQueuePolicyTask*>(task);
      break;
    }
    case Method::kSetWorkOrchProcPolicy: {
      ar << *reinterpret_cast<SetWorkOrchProcPolicyTask*>(task);
      break;
    }
    case Method::kFlush: {
      ar << *reinterpret_cast<FlushTask*>(task);
      break;
    }
    case Method::kDomainSize: {
      ar << *reinterpret_cast<DomainSizeTask*>(task);
      break;
    }
  }
}
/** Deserialize a task when popping from remote queue */
void LoadEnd(u32 method, BinaryInputArchive<false> &ar, Task *task) override {
  switch (method) {
    case Method::kCreateTaskState: {
      ar >> *reinterpret_cast<CreateTaskStateTask*>(task);
      break;
    }
    case Method::kDestroyTaskState: {
      ar >> *reinterpret_cast<DestroyTaskStateTask*>(task);
      break;
    }
    case Method::kRegisterTaskLib: {
      ar >> *reinterpret_cast<RegisterTaskLibTask*>(task);
      break;
    }
    case Method::kDestroyTaskLib: {
      ar >> *reinterpret_cast<DestroyTaskLibTask*>(task);
      break;
    }
    case Method::kGetOrCreateTaskStateId: {
      ar >> *reinterpret_cast<GetOrCreateTaskStateIdTask*>(task);
      break;
    }
    case Method::kGetTaskStateId: {
      ar >> *reinterpret_cast<GetTaskStateIdTask*>(task);
      break;
    }
    case Method::kStopRuntime: {
      ar >> *reinterpret_cast<StopRuntimeTask*>(task);
      break;
    }
    case Method::kSetWorkOrchQueuePolicy: {
      ar >> *reinterpret_cast<SetWorkOrchQueuePolicyTask*>(task);
      break;
    }
    case Method::kSetWorkOrchProcPolicy: {
      ar >> *reinterpret_cast<SetWorkOrchProcPolicyTask*>(task);
      break;
    }
    case Method::kFlush: {
      ar >> *reinterpret_cast<FlushTask*>(task);
      break;
    }
    case Method::kDomainSize: {
      ar >> *reinterpret_cast<DomainSizeTask*>(task);
      break;
    }
  }
}
/** Deserialize a task when popping from remote queue */
TaskPointer LoadReplicaEnd(u32 method, BinaryInputArchive<false> &ar, Task *task) override {
  TaskPointer task_ptr;
  switch (method) {
    case Method::kCreateTaskState: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<CreateTaskStateTask>(task_ptr.shm_);
      task_ptr.ptr_->task_dup(*task);
      ar >> *reinterpret_cast<CreateTaskStateTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kDestroyTaskState: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<DestroyTaskStateTask>(task_ptr.shm_);
      task_ptr.ptr_->task_dup(*task);
      ar >> *reinterpret_cast<DestroyTaskStateTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kRegisterTaskLib: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<RegisterTaskLibTask>(task_ptr.shm_);
      task_ptr.ptr_->task_dup(*task);
      ar >> *reinterpret_cast<RegisterTaskLibTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kDestroyTaskLib: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<DestroyTaskLibTask>(task_ptr.shm_);
      task_ptr.ptr_->task_dup(*task);
      ar >> *reinterpret_cast<DestroyTaskLibTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kGetOrCreateTaskStateId: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<GetOrCreateTaskStateIdTask>(task_ptr.shm_);
      task_ptr.ptr_->task_dup(*task);
      ar >> *reinterpret_cast<GetOrCreateTaskStateIdTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kGetTaskStateId: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<GetTaskStateIdTask>(task_ptr.shm_);
      task_ptr.ptr_->task_dup(*task);
      ar >> *reinterpret_cast<GetTaskStateIdTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kStopRuntime: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<StopRuntimeTask>(task_ptr.shm_);
      task_ptr.ptr_->task_dup(*task);
      ar >> *reinterpret_cast<StopRuntimeTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kSetWorkOrchQueuePolicy: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<SetWorkOrchQueuePolicyTask>(task_ptr.shm_);
      task_ptr.ptr_->task_dup(*task);
      ar >> *reinterpret_cast<SetWorkOrchQueuePolicyTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kSetWorkOrchProcPolicy: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<SetWorkOrchProcPolicyTask>(task_ptr.shm_);
      task_ptr.ptr_->task_dup(*task);
      ar >> *reinterpret_cast<SetWorkOrchProcPolicyTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kFlush: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<FlushTask>(task_ptr.shm_);
      task_ptr.ptr_->task_dup(*task);
      ar >> *reinterpret_cast<FlushTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kDomainSize: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<DomainSizeTask>(task_ptr.shm_);
      task_ptr.ptr_->task_dup(*task);
      ar >> *reinterpret_cast<DomainSizeTask*>(task_ptr.ptr_);
      break;
    }
  }
  return task_ptr;
}

#endif  // HRUN_CHIMAERA_ADMIN_METHODS_H_