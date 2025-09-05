#ifndef MOD_NAME_AUTOGEN_LIB_EXEC_H_
#define MOD_NAME_AUTOGEN_LIB_EXEC_H_

/**
 * Auto-generated execution dispatcher for MOD_NAME ChiMod
 * Provides switch-case dispatch for all implemented methods
 */

#include <chimaera/chimaera.h>
#include "MOD_NAME_methods.h"
#include "../MOD_NAME_runtime.h"

namespace chimaera::MOD_NAME {

/**
 * Execute a method on the runtime
 */
inline void Run(Runtime* runtime, chi::u32 method, hipc::FullPtr<chi::Task> task, chi::RunContext& rctx) {
  switch (method) {
    case Method::kCreate: {
      runtime->Create(task.Cast<CreateTask>(), rctx);
      break;
    }
    case Method::kDestroy: {
      runtime->Destroy(task.Cast<DestroyTask>(), rctx);
      break;
    }
    case Method::kCustom: {
      runtime->Custom(task.Cast<CustomTask>(), rctx);
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
inline void SaveIn(Runtime* runtime, chi::u32 method, chi::TaskSaveInArchive& archive, hipc::FullPtr<chi::Task> task_ptr) {
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
    case Method::kCustom: {
      auto typed_task = task_ptr.Cast<CustomTask>();
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
inline void LoadIn(Runtime* runtime, chi::u32 method, chi::TaskLoadInArchive& archive, hipc::FullPtr<chi::Task> task_ptr) {
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
    case Method::kCustom: {
      auto typed_task = task_ptr.Cast<CustomTask>();
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
inline void SaveOut(Runtime* runtime, chi::u32 method, chi::TaskSaveOutArchive& archive, hipc::FullPtr<chi::Task> task_ptr) {
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
    case Method::kCustom: {
      auto typed_task = task_ptr.Cast<CustomTask>();
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
inline void LoadOut(Runtime* runtime, chi::u32 method, chi::TaskLoadOutArchive& archive, hipc::FullPtr<chi::Task> task_ptr) {
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
    case Method::kCustom: {
      auto typed_task = task_ptr.Cast<CustomTask>();
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
inline void Monitor(Runtime* runtime, chi::MonitorModeId mode, chi::u32 method,
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
    case Method::kCustom: {
      runtime->MonitorCustom(mode, task_ptr.Cast<CustomTask>(), rctx);
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
inline void Del(Runtime* runtime, chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) {
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
    case Method::kCustom: {
      ipc_manager->DelTask(task_ptr.Cast<CustomTask>());
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
inline void NewCopy(Runtime* runtime, chi::u32 method,
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
    case Method::kCustom: {
      // Allocate new task using SHM default constructor
      auto typed_task = ipc_manager->NewTask<CustomTask>();
      if (!typed_task.IsNull()) {
        // Use HSHM strong copy method for actual copying
        typed_task->shm_strong_copy_main(*orig_task.Cast<CustomTask>());
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

} // namespace chimaera::MOD_NAME

#endif // MOD_NAME_AUTOGEN_LIB_EXEC_H_
