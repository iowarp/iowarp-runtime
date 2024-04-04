#ifndef HRUN_SMALL_MESSAGE_LIB_EXEC_H_
#define HRUN_SMALL_MESSAGE_LIB_EXEC_H_

/** Execute a task */
void Run(u32 method, Task *task, RunContext &rctx) override {
  switch (method) {
    case Method::kConstruct: {
      Construct(reinterpret_cast<ConstructTask *>(task), rctx);
      break;
    }
    case Method::kDestruct: {
      Destruct(reinterpret_cast<DestructTask *>(task), rctx);
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
void Monitor(u32 mode, Task *task, RunContext &rctx) override {
  switch (task->method_) {
    case Method::kConstruct: {
      MonitorConstruct(mode, reinterpret_cast<ConstructTask *>(task), rctx);
      break;
    }
    case Method::kDestruct: {
      MonitorDestruct(mode, reinterpret_cast<DestructTask *>(task), rctx);
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
void Del(u32 method, Task *task) override {
  switch (method) {
    case Method::kConstruct: {
      HRUN_CLIENT->DelTask<ConstructTask>(reinterpret_cast<ConstructTask *>(task));
      break;
    }
    case Method::kDestruct: {
      HRUN_CLIENT->DelTask<DestructTask>(reinterpret_cast<DestructTask *>(task));
      break;
    }
    case Method::kMd: {
      HRUN_CLIENT->DelTask<MdTask>(reinterpret_cast<MdTask *>(task));
      break;
    }
    case Method::kIo: {
      HRUN_CLIENT->DelTask<IoTask>(reinterpret_cast<IoTask *>(task));
      break;
    }
  }
}
/** Duplicate a task */
void Dup(u32 method, Task *orig_task, std::vector<LPointer<Task>> &dups) override {
  switch (method) {
    case Method::kConstruct: {
      chm::CALL_DUPLICATE(reinterpret_cast<ConstructTask*>(orig_task), dups);
      break;
    }
    case Method::kDestruct: {
      chm::CALL_DUPLICATE(reinterpret_cast<DestructTask*>(orig_task), dups);
      break;
    }
    case Method::kMd: {
      chm::CALL_DUPLICATE(reinterpret_cast<MdTask*>(orig_task), dups);
      break;
    }
    case Method::kIo: {
      chm::CALL_DUPLICATE(reinterpret_cast<IoTask*>(orig_task), dups);
      break;
    }
  }
}
/** Register the duplicate output with the origin task */
void DupEnd(u32 method, u32 replica, Task *orig_task, Task *dup_task) override {
  switch (method) {
    case Method::kConstruct: {
      chm::CALL_DUPLICATE_END(replica, reinterpret_cast<ConstructTask*>(orig_task), reinterpret_cast<ConstructTask*>(dup_task));
      break;
    }
    case Method::kDestruct: {
      chm::CALL_DUPLICATE_END(replica, reinterpret_cast<DestructTask*>(orig_task), reinterpret_cast<DestructTask*>(dup_task));
      break;
    }
    case Method::kMd: {
      chm::CALL_DUPLICATE_END(replica, reinterpret_cast<MdTask*>(orig_task), reinterpret_cast<MdTask*>(dup_task));
      break;
    }
    case Method::kIo: {
      chm::CALL_DUPLICATE_END(replica, reinterpret_cast<IoTask*>(orig_task), reinterpret_cast<IoTask*>(dup_task));
      break;
    }
  }
}
/** Ensure there is space to store replicated outputs */
void ReplicateStart(u32 method, u32 count, Task *task) override {
  switch (method) {
    case Method::kConstruct: {
      chm::CALL_REPLICA_START(count, reinterpret_cast<ConstructTask*>(task));
      break;
    }
    case Method::kDestruct: {
      chm::CALL_REPLICA_START(count, reinterpret_cast<DestructTask*>(task));
      break;
    }
    case Method::kMd: {
      chm::CALL_REPLICA_START(count, reinterpret_cast<MdTask*>(task));
      break;
    }
    case Method::kIo: {
      chm::CALL_REPLICA_START(count, reinterpret_cast<IoTask*>(task));
      break;
    }
  }
}
/** Determine success and handle failures */
void ReplicateEnd(u32 method, Task *task) override {
  switch (method) {
    case Method::kConstruct: {
      chm::CALL_REPLICA_END(reinterpret_cast<ConstructTask*>(task));
      break;
    }
    case Method::kDestruct: {
      chm::CALL_REPLICA_END(reinterpret_cast<DestructTask*>(task));
      break;
    }
    case Method::kMd: {
      chm::CALL_REPLICA_END(reinterpret_cast<MdTask*>(task));
      break;
    }
    case Method::kIo: {
      chm::CALL_REPLICA_END(reinterpret_cast<IoTask*>(task));
      break;
    }
  }
}
/** Serialize a task when initially pushing into remote */
std::vector<DataTransfer> SaveStart(u32 method, BinaryOutputArchive<true> &ar, Task *task) override {
  switch (method) {
    case Method::kConstruct: {
      ar << *reinterpret_cast<ConstructTask*>(task);
      break;
    }
    case Method::kDestruct: {
      ar << *reinterpret_cast<DestructTask*>(task);
      break;
    }
    case Method::kMd: {
      ar << *reinterpret_cast<MdTask*>(task);
      break;
    }
    case Method::kIo: {
      ar << *reinterpret_cast<IoTask*>(task);
      break;
    }
  }
  return ar.Get();
}
/** Deserialize a task when popping from remote queue */
TaskPointer LoadStart(u32 method, BinaryInputArchive<true> &ar) override {
  TaskPointer task_ptr;
  switch (method) {
    case Method::kConstruct: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<ConstructTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<ConstructTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kDestruct: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<DestructTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<DestructTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kMd: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<MdTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<MdTask*>(task_ptr.ptr_);
      break;
    }
    case Method::kIo: {
      task_ptr.ptr_ = HRUN_CLIENT->NewEmptyTask<IoTask>(task_ptr.shm_);
      ar >> *reinterpret_cast<IoTask*>(task_ptr.ptr_);
      break;
    }
  }
  return task_ptr;
}
/** Serialize a task when returning from remote queue */
std::vector<DataTransfer> SaveEnd(u32 method, BinaryOutputArchive<false> &ar, Task *task) override {
  switch (method) {
    case Method::kConstruct: {
      ar << *reinterpret_cast<ConstructTask*>(task);
      break;
    }
    case Method::kDestruct: {
      ar << *reinterpret_cast<DestructTask*>(task);
      break;
    }
    case Method::kMd: {
      ar << *reinterpret_cast<MdTask*>(task);
      break;
    }
    case Method::kIo: {
      ar << *reinterpret_cast<IoTask*>(task);
      break;
    }
  }
  return ar.Get();
}
/** Deserialize a task when returning from remote queue */
void LoadEnd(u32 replica, u32 method, BinaryInputArchive<false> &ar, Task *task) override {
  switch (method) {
    case Method::kConstruct: {
      ar.Deserialize(replica, *reinterpret_cast<ConstructTask*>(task));
      break;
    }
    case Method::kDestruct: {
      ar.Deserialize(replica, *reinterpret_cast<DestructTask*>(task));
      break;
    }
    case Method::kMd: {
      ar.Deserialize(replica, *reinterpret_cast<MdTask*>(task));
      break;
    }
    case Method::kIo: {
      ar.Deserialize(replica, *reinterpret_cast<IoTask*>(task));
      break;
    }
  }
}
/** Get the grouping of the task */
u32 GetGroup(u32 method, Task *task, hshm::charbuf &group) override {
  switch (method) {
    case Method::kConstruct: {
      return reinterpret_cast<ConstructTask*>(task)->GetGroup(group);
    }
    case Method::kDestruct: {
      return reinterpret_cast<DestructTask*>(task)->GetGroup(group);
    }
    case Method::kMd: {
      return reinterpret_cast<MdTask*>(task)->GetGroup(group);
    }
    case Method::kIo: {
      return reinterpret_cast<IoTask*>(task)->GetGroup(group);
    }
  }
  return -1;
}
/** Check if two tasks apart of the same group must block */
bool CompareGroup(u32 method, Task *task1, Task *task2) override {
  switch (method) {
    case Method::kConstruct: {
      return chm::CALL_CMPGRP<ConstructTask>(
        reinterpret_cast<ConstructTask*>(task1), task2);
    }
    case Method::kDestruct: {
      return chm::CALL_CMPGRP<DestructTask>(
        reinterpret_cast<DestructTask*>(task1), task2);
    }
    case Method::kMd: {
      return chm::CALL_CMPGRP<MdTask>(
        reinterpret_cast<MdTask*>(task1), task2);
    }
    case Method::kIo: {
      return chm::CALL_CMPGRP<IoTask>(
        reinterpret_cast<IoTask*>(task1), task2);
    }
  }
  return false;
}

#endif  // HRUN_SMALL_MESSAGE_METHODS_H_