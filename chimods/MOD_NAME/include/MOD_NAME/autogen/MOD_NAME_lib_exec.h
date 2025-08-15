#ifndef MOD_NAME_AUTOGEN_LIB_EXEC_H_
#define MOD_NAME_AUTOGEN_LIB_EXEC_H_

/**
 * Auto-generated execution dispatcher for MOD_NAME
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
  if (method == Method::kCreate) {
    runtime->Create(task.Cast<CreateTask>(), rctx);
  } else if (method == Method::kCustom) {
    runtime->Custom(task.Cast<CustomTask>(), rctx);
  }
  // Unknown method - do nothing
}

/**
 * Monitor a method on the runtime
 */
inline void Monitor(Runtime* runtime, chi::MonitorModeId mode, chi::u32 method, 
                   hipc::FullPtr<chi::Task> task_ptr, chi::RunContext& rctx) {
  if (method == Method::kCreate) {
    runtime->MonitorCreate(mode, task_ptr.Cast<CreateTask>(), rctx);
  } else if (method == Method::kCustom) {
    runtime->MonitorCustom(mode, task_ptr.Cast<CustomTask>(), rctx);
  }
}

/**
 * Delete a task from shared memory
 * Uses IPC manager to properly deallocate the task
 */
inline void Del(Runtime* runtime, chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) {
  // Use IPC manager to deallocate task from shared memory
  auto* ipc_manager = CHI_IPC;
  
  if (method == Method::kCreate) {
    ipc_manager->DelTask(task_ptr.Cast<CreateTask>());
  } else if (method == Method::kCustom) {
    ipc_manager->DelTask(task_ptr.Cast<CustomTask>());
  } else {
    // For unknown methods, still try to delete from main segment
    ipc_manager->DelTask(task_ptr);
  }
  
  (void)runtime; // Runtime not needed for IPC-managed deletion
}

} // namespace chimaera::MOD_NAME

#endif // MOD_NAME_AUTOGEN_LIB_EXEC_H_