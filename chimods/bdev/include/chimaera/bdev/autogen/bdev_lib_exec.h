#ifndef BDEV_AUTOGEN_LIB_EXEC_H_
#define BDEV_AUTOGEN_LIB_EXEC_H_

/**
 * Auto-generated execution dispatcher for bdev ChiMod
 * Provides switch-case dispatch for all implemented methods
 */

#include <chimaera/chimaera.h>
#include "bdev_methods.h"
#include "../bdev_runtime.h"

namespace chimaera::bdev {

/**
 * Execute a method on the runtime
 */
inline void Run(Container* runtime, chi::u32 method, hipc::FullPtr<chi::Task> task, chi::RunContext& rctx) {
  switch (method) {
    case Method::kCreate: {
      runtime->Create(task.Cast<CreateTask>(), rctx);
      break;
    }
    case Method::kDestroy: {
      runtime->Destroy(task.Cast<DestroyTask>(), rctx);
      break;
    }
    case Method::kAllocate: {
      runtime->Allocate(task.Cast<AllocateTask>(), rctx);
      break;
    }
    case Method::kFree: {
      runtime->Free(task.Cast<FreeTask>(), rctx);
      break;
    }
    case Method::kWrite: {
      runtime->Write(task.Cast<WriteTask>(), rctx);
      break;
    }
    case Method::kRead: {
      runtime->Read(task.Cast<ReadTask>(), rctx);
      break;
    }
    case Method::kStat: {
      runtime->Stat(task.Cast<StatTask>(), rctx);
      break;
    }
    default: {
      // Unknown method - do nothing
      break;
    }
  }
}

/**
 * Save input data for a task (serialize task inputs)
 */
inline void SaveIn(Container* runtime, chi::u32 method, chi::TaskSaveInArchive& archive, hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate: {
      auto typed_task = task_ptr.Cast<CreateTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kDestroy: {
      auto typed_task = task_ptr.Cast<DestroyTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kAllocate: {
      auto typed_task = task_ptr.Cast<AllocateTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kFree: {
      auto typed_task = task_ptr.Cast<FreeTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kWrite: {
      auto typed_task = task_ptr.Cast<WriteTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kRead: {
      auto typed_task = task_ptr.Cast<ReadTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kStat: {
      auto typed_task = task_ptr.Cast<StatTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    default: {
      // Unknown method - do nothing
      break;
    }
  }
}

/**
 * Load input data for a task (deserialize task inputs)
 */
inline void LoadIn(Container* runtime, chi::u32 method, chi::TaskLoadInArchive& archive, hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate: {
      auto typed_task = task_ptr.Cast<CreateTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kDestroy: {
      auto typed_task = task_ptr.Cast<DestroyTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kAllocate: {
      auto typed_task = task_ptr.Cast<AllocateTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kFree: {
      auto typed_task = task_ptr.Cast<FreeTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kWrite: {
      auto typed_task = task_ptr.Cast<WriteTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kRead: {
      auto typed_task = task_ptr.Cast<ReadTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kStat: {
      auto typed_task = task_ptr.Cast<StatTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    default: {
      // Unknown method - do nothing
      break;
    }
  }
}

/**
 * Save output data for a task (serialize task outputs)
 */
inline void SaveOut(Container* runtime, chi::u32 method, chi::TaskSaveOutArchive& archive, hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate: {
      auto typed_task = task_ptr.Cast<CreateTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kDestroy: {
      auto typed_task = task_ptr.Cast<DestroyTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kAllocate: {
      auto typed_task = task_ptr.Cast<AllocateTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kFree: {
      auto typed_task = task_ptr.Cast<FreeTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kWrite: {
      auto typed_task = task_ptr.Cast<WriteTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kRead: {
      auto typed_task = task_ptr.Cast<ReadTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kStat: {
      auto typed_task = task_ptr.Cast<StatTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    default: {
      // Unknown method - do nothing
      break;
    }
  }
}

/**
 * Load output data for a task (deserialize task outputs)
 */
inline void LoadOut(Container* runtime, chi::u32 method, chi::TaskLoadOutArchive& archive, hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate: {
      auto typed_task = task_ptr.Cast<CreateTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kDestroy: {
      auto typed_task = task_ptr.Cast<DestroyTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kAllocate: {
      auto typed_task = task_ptr.Cast<AllocateTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kFree: {
      auto typed_task = task_ptr.Cast<FreeTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kWrite: {
      auto typed_task = task_ptr.Cast<WriteTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kRead: {
      auto typed_task = task_ptr.Cast<ReadTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kStat: {
      auto typed_task = task_ptr.Cast<StatTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    default: {
      // Unknown method - do nothing
      break;
    }
  }
}

/**
 * Monitor a method on the runtime
 */
inline void Monitor(Container* runtime, chi::MonitorModeId mode, chi::u32 method,
                   hipc::FullPtr<chi::Task> task_ptr, chi::RunContext& rctx) {
  switch (method) {
    case Method::kCreate: {
      runtime->MonitorCreate(mode, task_ptr.Cast<CreateTask>(), rctx);
      break;
    }
    case Method::kDestroy: {
      runtime->MonitorDestroy(mode, task_ptr.Cast<DestroyTask>(), rctx);
      break;
    }
    case Method::kAllocate: {
      runtime->MonitorAllocate(mode, task_ptr.Cast<AllocateTask>(), rctx);
      break;
    }
    case Method::kFree: {
      runtime->MonitorFree(mode, task_ptr.Cast<FreeTask>(), rctx);
      break;
    }
    case Method::kWrite: {
      runtime->MonitorWrite(mode, task_ptr.Cast<WriteTask>(), rctx);
      break;
    }
    case Method::kRead: {
      runtime->MonitorRead(mode, task_ptr.Cast<ReadTask>(), rctx);
      break;
    }
    case Method::kStat: {
      runtime->MonitorStat(mode, task_ptr.Cast<StatTask>(), rctx);
      break;
    }
    default: {
      // Unknown method - do nothing
      break;
    }
  }
}

/**
 * Delete a task from shared memory
 * Uses IPC manager to properly deallocate the task
 */
inline void Del(Container* runtime, chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) {
  // Use IPC manager to deallocate task from shared memory
  auto* ipc_manager = CHI_IPC;
  
  switch (method) {
    case Method::kCreate: {
      ipc_manager->DelTask(task_ptr.Cast<CreateTask>());
      break;
    }
    case Method::kDestroy: {
      ipc_manager->DelTask(task_ptr.Cast<DestroyTask>());
      break;
    }
    case Method::kAllocate: {
      ipc_manager->DelTask(task_ptr.Cast<AllocateTask>());
      break;
    }
    case Method::kFree: {
      ipc_manager->DelTask(task_ptr.Cast<FreeTask>());
      break;
    }
    case Method::kWrite: {
      ipc_manager->DelTask(task_ptr.Cast<WriteTask>());
      break;
    }
    case Method::kRead: {
      ipc_manager->DelTask(task_ptr.Cast<ReadTask>());
      break;
    }
    case Method::kStat: {
      ipc_manager->DelTask(task_ptr.Cast<StatTask>());
      break;
    }
    default: {
      // For unknown methods, still try to delete from main segment
      ipc_manager->DelTask(task_ptr);
      break;
    }
  }
  
  (void)runtime; // Runtime not needed for IPC-managed deletion
}

/**
 * Create a new copy of a task (deep copy for distributed execution)
 */
inline void NewCopy(Container* runtime, chi::u32 method,
                    const hipc::FullPtr<chi::Task>& orig_task,
                    hipc::FullPtr<chi::Task>& dup_task, bool deep) {
  auto* ipc_manager = CHI_IPC;
  if (!ipc_manager) {
    return;
  }
  
  switch (method) {
    case Method::kCreate: {
      // Allocate new task using SHM default constructor
      auto typed_task = ipc_manager->NewTask<CreateTask>();
      if (!typed_task.IsNull()) {
        // Use HSHM strong copy method for actual copying
        typed_task->shm_strong_copy_main(*orig_task.Cast<CreateTask>());
        // Cast to base Task type for return
        dup_task = typed_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kDestroy: {
      // Allocate new task using SHM default constructor
      auto typed_task = ipc_manager->NewTask<DestroyTask>();
      if (!typed_task.IsNull()) {
        // Use HSHM strong copy method for actual copying
        typed_task->shm_strong_copy_main(*orig_task.Cast<DestroyTask>());
        // Cast to base Task type for return
        dup_task = typed_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kAllocate: {
      // Allocate new task using SHM default constructor
      auto typed_task = ipc_manager->NewTask<AllocateTask>();
      if (!typed_task.IsNull()) {
        // Use HSHM strong copy method for actual copying
        typed_task->shm_strong_copy_main(*orig_task.Cast<AllocateTask>());
        // Cast to base Task type for return
        dup_task = typed_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kFree: {
      // Allocate new task using SHM default constructor
      auto typed_task = ipc_manager->NewTask<FreeTask>();
      if (!typed_task.IsNull()) {
        // Use HSHM strong copy method for actual copying
        typed_task->shm_strong_copy_main(*orig_task.Cast<FreeTask>());
        // Cast to base Task type for return
        dup_task = typed_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kWrite: {
      // Allocate new task using SHM default constructor
      auto typed_task = ipc_manager->NewTask<WriteTask>();
      if (!typed_task.IsNull()) {
        // Use HSHM strong copy method for actual copying
        typed_task->shm_strong_copy_main(*orig_task.Cast<WriteTask>());
        // Cast to base Task type for return
        dup_task = typed_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kRead: {
      // Allocate new task using SHM default constructor
      auto typed_task = ipc_manager->NewTask<ReadTask>();
      if (!typed_task.IsNull()) {
        // Use HSHM strong copy method for actual copying
        typed_task->shm_strong_copy_main(*orig_task.Cast<ReadTask>());
        // Cast to base Task type for return
        dup_task = typed_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kStat: {
      // Allocate new task using SHM default constructor
      auto typed_task = ipc_manager->NewTask<StatTask>();
      if (!typed_task.IsNull()) {
        // Use HSHM strong copy method for actual copying
        typed_task->shm_strong_copy_main(*orig_task.Cast<StatTask>());
        // Cast to base Task type for return
        dup_task = typed_task.template Cast<chi::Task>();
      }
      break;
    }
    default: {
      // For unknown methods, create base Task copy
      auto typed_task = ipc_manager->NewTask<chi::Task>();
      if (!typed_task.IsNull()) {
        typed_task->shm_strong_copy_main(*orig_task);
        dup_task = typed_task;  // Already chi::Task type
      }
      break;
    }
  }
  
  (void)runtime; // Runtime not needed for IPC-managed allocation
  (void)deep;    // Deep copy parameter reserved for future use
}

} // namespace chimaera::bdev

#endif // BDEV_AUTOGEN_LIB_EXEC_H_
