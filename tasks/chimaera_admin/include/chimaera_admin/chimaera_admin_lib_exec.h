#ifndef HRUN_CHIMAERA_ADMIN_LIB_EXEC_H_
#define HRUN_CHIMAERA_ADMIN_LIB_EXEC_H_

/** Execute a task */
void Run(u32 method, Task *task, RunContext &rctx) override {
  switch (method) {
    case Method::kCreateContainer: {
      CreateContainer(reinterpret_cast<CreateContainerTask *>(task), rctx);
      break;
    }
    case Method::kDestroyContainer: {
      DestroyContainer(reinterpret_cast<DestroyContainerTask *>(task), rctx);
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
    case Method::kGetPoolId: {
      GetPoolId(reinterpret_cast<GetPoolIdTask *>(task), rctx);
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
    case Method::kCreateContainer: {
      MonitorCreateContainer(mode, reinterpret_cast<CreateContainerTask *>(task), rctx);
      break;
    }
    case Method::kDestroyContainer: {
      MonitorDestroyContainer(mode, reinterpret_cast<DestroyContainerTask *>(task), rctx);
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
    case Method::kGetPoolId: {
      MonitorGetPoolId(mode, reinterpret_cast<GetPoolIdTask *>(task), rctx);
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
    case Method::kCreateContainer: {
      CHI_CLIENT->DelTask<CreateContainerTask>(reinterpret_cast<CreateContainerTask *>(task));
      break;
    }
    case Method::kDestroyContainer: {
      CHI_CLIENT->DelTask<DestroyContainerTask>(reinterpret_cast<DestroyContainerTask *>(task));
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
    case Method::kGetPoolId: {
      CHI_CLIENT->DelTask<GetPoolIdTask>(reinterpret_cast<GetPoolIdTask *>(task));
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
    case Method::kCreateContainer: {
      chi::CALL_COPY_START(reinterpret_cast<CreateContainerTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kDestroyContainer: {
      chi::CALL_COPY_START(reinterpret_cast<DestroyContainerTask*>(orig_task), dup_task, deep);
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
    case Method::kGetPoolId: {
      chi::CALL_COPY_START(reinterpret_cast<GetPoolIdTask*>(orig_task), dup_task, deep);
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
    case Method::kCreateContainer: {
      ar << *reinterpret_cast<CreateContainerTask*>(task);
      break;
    }
    case Method::kDestroyContainer: {
      ar << *reinterpret_cast<DestroyContainerTask*>(task);
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
    case Method::kGetPoolId: {
      ar << *reinterpret_cast<GetPoolIdTask*>(task);
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
    case Method::kCreateContainer: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<CreateContainerTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<CreateContainerTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kDestroyContainer: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<DestroyContainerTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<DestroyContainerTask*>(task_ptr.ptr_);
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
    case Method::kGetPoolId: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<GetPoolIdTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<GetPoolIdTask*>(task_ptr.ptr_);
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
    case Method::kCreateContainer: {
      ar << *reinterpret_cast<CreateContainerTask*>(task);
      break;
    }
    case Method::kDestroyContainer: {
      ar << *reinterpret_cast<DestroyContainerTask*>(task);
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
    case Method::kGetPoolId: {
      ar << *reinterpret_cast<GetPoolIdTask*>(task);
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
    case Method::kCreateContainer: {
      ar >> *reinterpret_cast<CreateContainerTask*>(task);
      break;
    }
    case Method::kDestroyContainer: {
      ar >> *reinterpret_cast<DestroyContainerTask*>(task);
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
    case Method::kGetPoolId: {
      ar >> *reinterpret_cast<GetPoolIdTask*>(task);
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