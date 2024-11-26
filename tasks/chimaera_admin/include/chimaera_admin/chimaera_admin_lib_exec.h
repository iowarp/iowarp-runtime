#ifndef CHI_CHIMAERA_ADMIN_LIB_EXEC_H_
#define CHI_CHIMAERA_ADMIN_LIB_EXEC_H_

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
    case Method::kCreateContainer: {
      CreateContainer(reinterpret_cast<CreateContainerTask *>(task), rctx);
      break;
    }
    case Method::kDestroyContainer: {
      DestroyContainer(reinterpret_cast<DestroyContainerTask *>(task), rctx);
      break;
    }
    case Method::kRegisterModule: {
      RegisterModule(reinterpret_cast<RegisterModuleTask *>(task), rctx);
      break;
    }
    case Method::kDestroyModule: {
      DestroyModule(reinterpret_cast<DestroyModuleTask *>(task), rctx);
      break;
    }
    case Method::kUpgradeModule: {
      UpgradeModule(reinterpret_cast<UpgradeModuleTask *>(task), rctx);
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
    case Method::kCreateContainer: {
      MonitorCreateContainer(mode, reinterpret_cast<CreateContainerTask *>(task), rctx);
      break;
    }
    case Method::kDestroyContainer: {
      MonitorDestroyContainer(mode, reinterpret_cast<DestroyContainerTask *>(task), rctx);
      break;
    }
    case Method::kRegisterModule: {
      MonitorRegisterModule(mode, reinterpret_cast<RegisterModuleTask *>(task), rctx);
      break;
    }
    case Method::kDestroyModule: {
      MonitorDestroyModule(mode, reinterpret_cast<DestroyModuleTask *>(task), rctx);
      break;
    }
    case Method::kUpgradeModule: {
      MonitorUpgradeModule(mode, reinterpret_cast<UpgradeModuleTask *>(task), rctx);
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
    case Method::kCreateContainer: {
      CHI_CLIENT->DelTask<CreateContainerTask>(mctx, reinterpret_cast<CreateContainerTask *>(task));
      break;
    }
    case Method::kDestroyContainer: {
      CHI_CLIENT->DelTask<DestroyContainerTask>(mctx, reinterpret_cast<DestroyContainerTask *>(task));
      break;
    }
    case Method::kRegisterModule: {
      CHI_CLIENT->DelTask<RegisterModuleTask>(mctx, reinterpret_cast<RegisterModuleTask *>(task));
      break;
    }
    case Method::kDestroyModule: {
      CHI_CLIENT->DelTask<DestroyModuleTask>(mctx, reinterpret_cast<DestroyModuleTask *>(task));
      break;
    }
    case Method::kUpgradeModule: {
      CHI_CLIENT->DelTask<UpgradeModuleTask>(mctx, reinterpret_cast<UpgradeModuleTask *>(task));
      break;
    }
    case Method::kGetPoolId: {
      CHI_CLIENT->DelTask<GetPoolIdTask>(mctx, reinterpret_cast<GetPoolIdTask *>(task));
      break;
    }
    case Method::kStopRuntime: {
      CHI_CLIENT->DelTask<StopRuntimeTask>(mctx, reinterpret_cast<StopRuntimeTask *>(task));
      break;
    }
    case Method::kSetWorkOrchQueuePolicy: {
      CHI_CLIENT->DelTask<SetWorkOrchQueuePolicyTask>(mctx, reinterpret_cast<SetWorkOrchQueuePolicyTask *>(task));
      break;
    }
    case Method::kSetWorkOrchProcPolicy: {
      CHI_CLIENT->DelTask<SetWorkOrchProcPolicyTask>(mctx, reinterpret_cast<SetWorkOrchProcPolicyTask *>(task));
      break;
    }
    case Method::kFlush: {
      CHI_CLIENT->DelTask<FlushTask>(mctx, reinterpret_cast<FlushTask *>(task));
      break;
    }
    case Method::kGetDomainSize: {
      CHI_CLIENT->DelTask<GetDomainSizeTask>(mctx, reinterpret_cast<GetDomainSizeTask *>(task));
      break;
    }
    case Method::kUpdateDomain: {
      CHI_CLIENT->DelTask<UpdateDomainTask>(mctx, reinterpret_cast<UpdateDomainTask *>(task));
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
    case Method::kCreateContainer: {
      chi::CALL_COPY_START(
        reinterpret_cast<const CreateContainerTask*>(orig_task), 
        reinterpret_cast<CreateContainerTask*>(dup_task), deep);
      break;
    }
    case Method::kDestroyContainer: {
      chi::CALL_COPY_START(
        reinterpret_cast<const DestroyContainerTask*>(orig_task), 
        reinterpret_cast<DestroyContainerTask*>(dup_task), deep);
      break;
    }
    case Method::kRegisterModule: {
      chi::CALL_COPY_START(
        reinterpret_cast<const RegisterModuleTask*>(orig_task), 
        reinterpret_cast<RegisterModuleTask*>(dup_task), deep);
      break;
    }
    case Method::kDestroyModule: {
      chi::CALL_COPY_START(
        reinterpret_cast<const DestroyModuleTask*>(orig_task), 
        reinterpret_cast<DestroyModuleTask*>(dup_task), deep);
      break;
    }
    case Method::kUpgradeModule: {
      chi::CALL_COPY_START(
        reinterpret_cast<const UpgradeModuleTask*>(orig_task), 
        reinterpret_cast<UpgradeModuleTask*>(dup_task), deep);
      break;
    }
    case Method::kGetPoolId: {
      chi::CALL_COPY_START(
        reinterpret_cast<const GetPoolIdTask*>(orig_task), 
        reinterpret_cast<GetPoolIdTask*>(dup_task), deep);
      break;
    }
    case Method::kStopRuntime: {
      chi::CALL_COPY_START(
        reinterpret_cast<const StopRuntimeTask*>(orig_task), 
        reinterpret_cast<StopRuntimeTask*>(dup_task), deep);
      break;
    }
    case Method::kSetWorkOrchQueuePolicy: {
      chi::CALL_COPY_START(
        reinterpret_cast<const SetWorkOrchQueuePolicyTask*>(orig_task), 
        reinterpret_cast<SetWorkOrchQueuePolicyTask*>(dup_task), deep);
      break;
    }
    case Method::kSetWorkOrchProcPolicy: {
      chi::CALL_COPY_START(
        reinterpret_cast<const SetWorkOrchProcPolicyTask*>(orig_task), 
        reinterpret_cast<SetWorkOrchProcPolicyTask*>(dup_task), deep);
      break;
    }
    case Method::kFlush: {
      chi::CALL_COPY_START(
        reinterpret_cast<const FlushTask*>(orig_task), 
        reinterpret_cast<FlushTask*>(dup_task), deep);
      break;
    }
    case Method::kGetDomainSize: {
      chi::CALL_COPY_START(
        reinterpret_cast<const GetDomainSizeTask*>(orig_task), 
        reinterpret_cast<GetDomainSizeTask*>(dup_task), deep);
      break;
    }
    case Method::kUpdateDomain: {
      chi::CALL_COPY_START(
        reinterpret_cast<const UpdateDomainTask*>(orig_task), 
        reinterpret_cast<UpdateDomainTask*>(dup_task), deep);
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
    case Method::kCreateContainer: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const CreateContainerTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kDestroyContainer: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const DestroyContainerTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kRegisterModule: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const RegisterModuleTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kDestroyModule: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const DestroyModuleTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kUpgradeModule: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const UpgradeModuleTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kGetPoolId: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const GetPoolIdTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kStopRuntime: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const StopRuntimeTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kSetWorkOrchQueuePolicy: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const SetWorkOrchQueuePolicyTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kSetWorkOrchProcPolicy: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const SetWorkOrchProcPolicyTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kFlush: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const FlushTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kGetDomainSize: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const GetDomainSizeTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kUpdateDomain: {
      chi::CALL_NEW_COPY_START(reinterpret_cast<const UpdateDomainTask*>(orig_task), dup_task, deep);
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
    case Method::kCreateContainer: {
      ar << *reinterpret_cast<CreateContainerTask*>(task);
      break;
    }
    case Method::kDestroyContainer: {
      ar << *reinterpret_cast<DestroyContainerTask*>(task);
      break;
    }
    case Method::kRegisterModule: {
      ar << *reinterpret_cast<RegisterModuleTask*>(task);
      break;
    }
    case Method::kDestroyModule: {
      ar << *reinterpret_cast<DestroyModuleTask*>(task);
      break;
    }
    case Method::kUpgradeModule: {
      ar << *reinterpret_cast<UpgradeModuleTask*>(task);
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
    case Method::kCreateContainer: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<CreateContainerTask>(
             HSHM_DEFAULT_MEM_CTX, task_ptr.shm_);
      ar >> *reinterpret_cast<CreateContainerTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kDestroyContainer: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<DestroyContainerTask>(
             HSHM_DEFAULT_MEM_CTX, task_ptr.shm_);
      ar >> *reinterpret_cast<DestroyContainerTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kRegisterModule: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<RegisterModuleTask>(
             HSHM_DEFAULT_MEM_CTX, task_ptr.shm_);
      ar >> *reinterpret_cast<RegisterModuleTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kDestroyModule: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<DestroyModuleTask>(
             HSHM_DEFAULT_MEM_CTX, task_ptr.shm_);
      ar >> *reinterpret_cast<DestroyModuleTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kUpgradeModule: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<UpgradeModuleTask>(
             HSHM_DEFAULT_MEM_CTX, task_ptr.shm_);
      ar >> *reinterpret_cast<UpgradeModuleTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kGetPoolId: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<GetPoolIdTask>(
             HSHM_DEFAULT_MEM_CTX, task_ptr.shm_);
      ar >> *reinterpret_cast<GetPoolIdTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kStopRuntime: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<StopRuntimeTask>(
             HSHM_DEFAULT_MEM_CTX, task_ptr.shm_);
      ar >> *reinterpret_cast<StopRuntimeTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kSetWorkOrchQueuePolicy: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<SetWorkOrchQueuePolicyTask>(
             HSHM_DEFAULT_MEM_CTX, task_ptr.shm_);
      ar >> *reinterpret_cast<SetWorkOrchQueuePolicyTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kSetWorkOrchProcPolicy: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<SetWorkOrchProcPolicyTask>(
             HSHM_DEFAULT_MEM_CTX, task_ptr.shm_);
      ar >> *reinterpret_cast<SetWorkOrchProcPolicyTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kFlush: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<FlushTask>(
             HSHM_DEFAULT_MEM_CTX, task_ptr.shm_);
      ar >> *reinterpret_cast<FlushTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kGetDomainSize: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<GetDomainSizeTask>(
             HSHM_DEFAULT_MEM_CTX, task_ptr.shm_);
      ar >> *reinterpret_cast<GetDomainSizeTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kUpdateDomain: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<UpdateDomainTask>(
             HSHM_DEFAULT_MEM_CTX, task_ptr.shm_);
      ar >> *reinterpret_cast<UpdateDomainTask*>(task_ptr.ptr_);
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
    case Method::kCreateContainer: {
      ar << *reinterpret_cast<CreateContainerTask*>(task);
      break;
    }
    case Method::kDestroyContainer: {
      ar << *reinterpret_cast<DestroyContainerTask*>(task);
      break;
    }
    case Method::kRegisterModule: {
      ar << *reinterpret_cast<RegisterModuleTask*>(task);
      break;
    }
    case Method::kDestroyModule: {
      ar << *reinterpret_cast<DestroyModuleTask*>(task);
      break;
    }
    case Method::kUpgradeModule: {
      ar << *reinterpret_cast<UpgradeModuleTask*>(task);
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
    case Method::kCreate: {
      ar >> *reinterpret_cast<CreateTask*>(task);
      break;
    }
    case Method::kDestroy: {
      ar >> *reinterpret_cast<DestroyTask*>(task);
      break;
    }
    case Method::kCreateContainer: {
      ar >> *reinterpret_cast<CreateContainerTask*>(task);
      break;
    }
    case Method::kDestroyContainer: {
      ar >> *reinterpret_cast<DestroyContainerTask*>(task);
      break;
    }
    case Method::kRegisterModule: {
      ar >> *reinterpret_cast<RegisterModuleTask*>(task);
      break;
    }
    case Method::kDestroyModule: {
      ar >> *reinterpret_cast<DestroyModuleTask*>(task);
      break;
    }
    case Method::kUpgradeModule: {
      ar >> *reinterpret_cast<UpgradeModuleTask*>(task);
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

#endif  // CHI_CHIMAERA_ADMIN_LIB_EXEC_H_