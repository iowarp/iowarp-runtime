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
    case Method::kGetDomainSize: {
      GetDomainSize(reinterpret_cast<GetDomainSizeTask *>(task), rctx);
      break;
    }
    case Method::kUpdateDomain: {
      UpdateDomain(reinterpret_cast<UpdateDomainTask *>(task), rctx);
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
    case Method::kGetDomainSize: {
      MonitorGetDomainSize(mode, reinterpret_cast<GetDomainSizeTask *>(task), rctx);
      break;
    }
    case Method::kUpdateDomain: {
      MonitorUpdateDomain(mode, reinterpret_cast<UpdateDomainTask *>(task), rctx);
      break;
    }
  }
}
/** Delete a task */
void Del(u32 method, Task *task) override {
  switch (method) {
    case Method::kCreateTaskState: {
      CHI_CLIENT->DelTask<CreateTaskStateTask>(reinterpret_cast<CreateTaskStateTask *>(task));
      break;
    }
    case Method::kDestroyTaskState: {
      CHI_CLIENT->DelTask<DestroyTaskStateTask>(reinterpret_cast<DestroyTaskStateTask *>(task));
      break;
    }
    case Method::kRegisterTaskLib: {
      CHI_CLIENT->DelTask<RegisterTaskLibTask>(reinterpret_cast<RegisterTaskLibTask *>(task));
      break;
    }
    case Method::kDestroyTaskLib: {
      CHI_CLIENT->DelTask<DestroyTaskLibTask>(reinterpret_cast<DestroyTaskLibTask *>(task));
      break;
    }
    case Method::kGetTaskStateId: {
      CHI_CLIENT->DelTask<GetTaskStateIdTask>(reinterpret_cast<GetTaskStateIdTask *>(task));
      break;
    }
    case Method::kStopRuntime: {
      CHI_CLIENT->DelTask<StopRuntimeTask>(reinterpret_cast<StopRuntimeTask *>(task));
      break;
    }
    case Method::kSetWorkOrchQueuePolicy: {
      CHI_CLIENT->DelTask<SetWorkOrchQueuePolicyTask>(reinterpret_cast<SetWorkOrchQueuePolicyTask *>(task));
      break;
    }
    case Method::kSetWorkOrchProcPolicy: {
      CHI_CLIENT->DelTask<SetWorkOrchProcPolicyTask>(reinterpret_cast<SetWorkOrchProcPolicyTask *>(task));
      break;
    }
    case Method::kFlush: {
      CHI_CLIENT->DelTask<FlushTask>(reinterpret_cast<FlushTask *>(task));
      break;
    }
    case Method::kGetDomainSize: {
      CHI_CLIENT->DelTask<GetDomainSizeTask>(reinterpret_cast<GetDomainSizeTask *>(task));
      break;
    }
    case Method::kUpdateDomain: {
      CHI_CLIENT->DelTask<UpdateDomainTask>(reinterpret_cast<UpdateDomainTask *>(task));
      break;
    }
  }
}
/** Duplicate a task */
void CopyStart(u32 method, Task *orig_task, LPointer<Task> &dup_task, bool deep) override {
  switch (method) {
    case Method::kCreateTaskState: {
      chi::CALL_COPY_START(reinterpret_cast<CreateTaskStateTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kDestroyTaskState: {
      chi::CALL_COPY_START(reinterpret_cast<DestroyTaskStateTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kRegisterTaskLib: {
      chi::CALL_COPY_START(reinterpret_cast<RegisterTaskLibTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kDestroyTaskLib: {
      chi::CALL_COPY_START(reinterpret_cast<DestroyTaskLibTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kGetTaskStateId: {
      chi::CALL_COPY_START(reinterpret_cast<GetTaskStateIdTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kStopRuntime: {
      chi::CALL_COPY_START(reinterpret_cast<StopRuntimeTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kSetWorkOrchQueuePolicy: {
      chi::CALL_COPY_START(reinterpret_cast<SetWorkOrchQueuePolicyTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kSetWorkOrchProcPolicy: {
      chi::CALL_COPY_START(reinterpret_cast<SetWorkOrchProcPolicyTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kFlush: {
      chi::CALL_COPY_START(reinterpret_cast<FlushTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kGetDomainSize: {
      chi::CALL_COPY_START(reinterpret_cast<GetDomainSizeTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kUpdateDomain: {
      chi::CALL_COPY_START(reinterpret_cast<UpdateDomainTask*>(orig_task), dup_task, deep);
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
    case Method::kGetDomainSize: {
      ar << *reinterpret_cast<GetDomainSizeTask*>(task);
      break;
    }
    case Method::kUpdateDomain: {
      ar << *reinterpret_cast<UpdateDomainTask*>(task);
      break;
    }
  }
}
/** Deserialize a task when popping from remote queue */
TaskPointer LoadStart(u32 method, BinaryInputArchive<true> &ar) override {
  TaskPointer task_ptr;
  switch (method) {
    case Method::kCreateTaskState: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<CreateTaskStateTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<CreateTaskStateTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kDestroyTaskState: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<DestroyTaskStateTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<DestroyTaskStateTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kRegisterTaskLib: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<RegisterTaskLibTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<RegisterTaskLibTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kDestroyTaskLib: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<DestroyTaskLibTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<DestroyTaskLibTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kGetTaskStateId: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<GetTaskStateIdTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<GetTaskStateIdTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kStopRuntime: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<StopRuntimeTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<StopRuntimeTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kSetWorkOrchQueuePolicy: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<SetWorkOrchQueuePolicyTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<SetWorkOrchQueuePolicyTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kSetWorkOrchProcPolicy: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<SetWorkOrchProcPolicyTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<SetWorkOrchProcPolicyTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kFlush: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<FlushTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<FlushTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kGetDomainSize: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<GetDomainSizeTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<GetDomainSizeTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kUpdateDomain: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<UpdateDomainTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<UpdateDomainTask*>(task_ptr.ptr_);
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
    case Method::kGetDomainSize: {
      ar << *reinterpret_cast<GetDomainSizeTask*>(task);
      break;
    }
    case Method::kUpdateDomain: {
      ar << *reinterpret_cast<UpdateDomainTask*>(task);
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
    case Method::kGetDomainSize: {
      ar >> *reinterpret_cast<GetDomainSizeTask*>(task);
      break;
    }
    case Method::kUpdateDomain: {
      ar >> *reinterpret_cast<UpdateDomainTask*>(task);
      break;
    }
  }
}

#endif  // HRUN_CHIMAERA_ADMIN_METHODS_H_