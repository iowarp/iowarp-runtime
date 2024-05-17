#ifndef HRUN_WORCH_QUEUE_ROUND_ROBIN_LIB_EXEC_H_
#define HRUN_WORCH_QUEUE_ROUND_ROBIN_LIB_EXEC_H_

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
    case Method::kSchedule: {
      Schedule(reinterpret_cast<ScheduleTask *>(task), rctx);
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
    case Method::kSchedule: {
      MonitorSchedule(mode, reinterpret_cast<ScheduleTask *>(task), rctx);
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
    case Method::kSchedule: {
      CHI_CLIENT->DelTask<ScheduleTask>(reinterpret_cast<ScheduleTask *>(task));
      break;
    }
  }
}
/** Duplicate a task */
void CopyStart(u32 method, Task *orig_task, LPointer<Task> &dup_task, bool deep) override {
  switch (method) {
    case Method::kCreate: {
      chi::CALL_COPY_START(reinterpret_cast<CreateTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kDestruct: {
      chi::CALL_COPY_START(reinterpret_cast<DestructTask*>(orig_task), dup_task, deep);
      break;
    }
    case Method::kSchedule: {
      chi::CALL_COPY_START(reinterpret_cast<ScheduleTask*>(orig_task), dup_task, deep);
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
    case Method::kSchedule: {
      ar << *reinterpret_cast<ScheduleTask*>(task);
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
    case Method::kSchedule: {
      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<ScheduleTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<ScheduleTask*>(task_ptr.ptr_);
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
    case Method::kSchedule: {
      ar << *reinterpret_cast<ScheduleTask*>(task);
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
    case Method::kSchedule: {
      ar >> *reinterpret_cast<ScheduleTask*>(task);
      break;
    }
  }
}

#endif  // HRUN_WORCH_QUEUE_ROUND_ROBIN_METHODS_H_