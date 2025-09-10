#ifndef ADMIN_AUTOGEN_LIB_EXEC_H_
#define ADMIN_AUTOGEN_LIB_EXEC_H_

/**
 * Auto-generated execution dispatcher for admin ChiMod
 * Provides switch-case dispatch for all implemented methods
 */

#include <chimaera/chimaera.h>
#include "admin_methods.h"
#include "../admin_runtime.h"

namespace chimaera::admin {

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
    case Method::kGetOrCreatePool: {
      runtime->GetOrCreatePool(task.Cast<admin::GetOrCreatePoolTask<admin::CreateParams>>(), rctx);
      break;
    }
    case Method::kDestroyPool: {
      runtime->DestroyPool(task.Cast<DestroyPoolTask>(), rctx);
      break;
    }
    case Method::kStopRuntime: {
      runtime->StopRuntime(task.Cast<StopRuntimeTask>(), rctx);
      break;
    }
    case Method::kFlush: {
      runtime->Flush(task.Cast<FlushTask>(), rctx);
      break;
    }
    case Method::kClientSendTaskIn: {
      runtime->ClientSendTaskIn(task.Cast<ClientSendTaskInTask>(), rctx);
      break;
    }
    case Method::kServerRecvTaskIn: {
      runtime->ServerRecvTaskIn(task.Cast<ServerRecvTaskInTask>(), rctx);
      break;
    }
    case Method::kServerSendTaskOut: {
      runtime->ServerSendTaskOut(task.Cast<ServerSendTaskOutTask>(), rctx);
      break;
    }
    case Method::kClientRecvTaskOut: {
      runtime->ClientRecvTaskOut(task.Cast<ClientRecvTaskOutTask>(), rctx);
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
    case Method::kGetOrCreatePool: {
      auto typed_task = task_ptr.Cast<admin::GetOrCreatePoolTask<admin::CreateParams>>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kDestroyPool: {
      auto typed_task = task_ptr.Cast<DestroyPoolTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kStopRuntime: {
      auto typed_task = task_ptr.Cast<StopRuntimeTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kFlush: {
      auto typed_task = task_ptr.Cast<FlushTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kClientSendTaskIn: {
      auto typed_task = task_ptr.Cast<ClientSendTaskInTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kServerRecvTaskIn: {
      auto typed_task = task_ptr.Cast<ServerRecvTaskInTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kServerSendTaskOut: {
      auto typed_task = task_ptr.Cast<ServerSendTaskOutTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kClientRecvTaskOut: {
      auto typed_task = task_ptr.Cast<ClientRecvTaskOutTask>();
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
    case Method::kGetOrCreatePool: {
      auto typed_task = task_ptr.Cast<admin::GetOrCreatePoolTask<admin::CreateParams>>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kDestroyPool: {
      auto typed_task = task_ptr.Cast<DestroyPoolTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kStopRuntime: {
      auto typed_task = task_ptr.Cast<StopRuntimeTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kFlush: {
      auto typed_task = task_ptr.Cast<FlushTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kClientSendTaskIn: {
      auto typed_task = task_ptr.Cast<ClientSendTaskInTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kServerRecvTaskIn: {
      auto typed_task = task_ptr.Cast<ServerRecvTaskInTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kServerSendTaskOut: {
      auto typed_task = task_ptr.Cast<ServerSendTaskOutTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kClientRecvTaskOut: {
      auto typed_task = task_ptr.Cast<ClientRecvTaskOutTask>();
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
    case Method::kGetOrCreatePool: {
      auto typed_task = task_ptr.Cast<admin::GetOrCreatePoolTask<admin::CreateParams>>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kDestroyPool: {
      auto typed_task = task_ptr.Cast<DestroyPoolTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kStopRuntime: {
      auto typed_task = task_ptr.Cast<StopRuntimeTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kFlush: {
      auto typed_task = task_ptr.Cast<FlushTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kClientSendTaskIn: {
      auto typed_task = task_ptr.Cast<ClientSendTaskInTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kServerRecvTaskIn: {
      auto typed_task = task_ptr.Cast<ServerRecvTaskInTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kServerSendTaskOut: {
      auto typed_task = task_ptr.Cast<ServerSendTaskOutTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kClientRecvTaskOut: {
      auto typed_task = task_ptr.Cast<ClientRecvTaskOutTask>();
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
    case Method::kGetOrCreatePool: {
      auto typed_task = task_ptr.Cast<admin::GetOrCreatePoolTask<admin::CreateParams>>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kDestroyPool: {
      auto typed_task = task_ptr.Cast<DestroyPoolTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kStopRuntime: {
      auto typed_task = task_ptr.Cast<StopRuntimeTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kFlush: {
      auto typed_task = task_ptr.Cast<FlushTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kClientSendTaskIn: {
      auto typed_task = task_ptr.Cast<ClientSendTaskInTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kServerRecvTaskIn: {
      auto typed_task = task_ptr.Cast<ServerRecvTaskInTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kServerSendTaskOut: {
      auto typed_task = task_ptr.Cast<ServerSendTaskOutTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kClientRecvTaskOut: {
      auto typed_task = task_ptr.Cast<ClientRecvTaskOutTask>();
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
    case Method::kGetOrCreatePool: {
      runtime->MonitorGetOrCreatePool(mode, task_ptr.Cast<admin::GetOrCreatePoolTask<admin::CreateParams>>(), rctx);
      break;
    }
    case Method::kDestroyPool: {
      runtime->MonitorDestroyPool(mode, task_ptr.Cast<DestroyPoolTask>(), rctx);
      break;
    }
    case Method::kStopRuntime: {
      runtime->MonitorStopRuntime(mode, task_ptr.Cast<StopRuntimeTask>(), rctx);
      break;
    }
    case Method::kFlush: {
      runtime->MonitorFlush(mode, task_ptr.Cast<FlushTask>(), rctx);
      break;
    }
    case Method::kClientSendTaskIn: {
      runtime->MonitorClientSendTaskIn(mode, task_ptr.Cast<ClientSendTaskInTask>(), rctx);
      break;
    }
    case Method::kServerRecvTaskIn: {
      runtime->MonitorServerRecvTaskIn(mode, task_ptr.Cast<ServerRecvTaskInTask>(), rctx);
      break;
    }
    case Method::kServerSendTaskOut: {
      runtime->MonitorServerSendTaskOut(mode, task_ptr.Cast<ServerSendTaskOutTask>(), rctx);
      break;
    }
    case Method::kClientRecvTaskOut: {
      runtime->MonitorClientRecvTaskOut(mode, task_ptr.Cast<ClientRecvTaskOutTask>(), rctx);
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
    case Method::kGetOrCreatePool: {
      ipc_manager->DelTask(task_ptr.Cast<admin::GetOrCreatePoolTask<admin::CreateParams>>());
      break;
    }
    case Method::kDestroyPool: {
      ipc_manager->DelTask(task_ptr.Cast<DestroyPoolTask>());
      break;
    }
    case Method::kStopRuntime: {
      ipc_manager->DelTask(task_ptr.Cast<StopRuntimeTask>());
      break;
    }
    case Method::kFlush: {
      ipc_manager->DelTask(task_ptr.Cast<FlushTask>());
      break;
    }
    case Method::kClientSendTaskIn: {
      ipc_manager->DelTask(task_ptr.Cast<ClientSendTaskInTask>());
      break;
    }
    case Method::kServerRecvTaskIn: {
      ipc_manager->DelTask(task_ptr.Cast<ServerRecvTaskInTask>());
      break;
    }
    case Method::kServerSendTaskOut: {
      ipc_manager->DelTask(task_ptr.Cast<ServerSendTaskOutTask>());
      break;
    }
    case Method::kClientRecvTaskOut: {
      ipc_manager->DelTask(task_ptr.Cast<ClientRecvTaskOutTask>());
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
    case Method::kGetOrCreatePool: {
      // Allocate new task using SHM default constructor
      auto typed_task = ipc_manager->NewTask<admin::GetOrCreatePoolTask<admin::CreateParams>>();
      if (!typed_task.IsNull()) {
        // Use HSHM strong copy method for actual copying
        typed_task->shm_strong_copy_main(*orig_task.Cast<admin::GetOrCreatePoolTask<admin::CreateParams>>());
        // Cast to base Task type for return
        dup_task = typed_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kDestroyPool: {
      // Allocate new task using SHM default constructor
      auto typed_task = ipc_manager->NewTask<DestroyPoolTask>();
      if (!typed_task.IsNull()) {
        // Use HSHM strong copy method for actual copying
        typed_task->shm_strong_copy_main(*orig_task.Cast<DestroyPoolTask>());
        // Cast to base Task type for return
        dup_task = typed_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kStopRuntime: {
      // Allocate new task using SHM default constructor
      auto typed_task = ipc_manager->NewTask<StopRuntimeTask>();
      if (!typed_task.IsNull()) {
        // Use HSHM strong copy method for actual copying
        typed_task->shm_strong_copy_main(*orig_task.Cast<StopRuntimeTask>());
        // Cast to base Task type for return
        dup_task = typed_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kFlush: {
      // Allocate new task using SHM default constructor
      auto typed_task = ipc_manager->NewTask<FlushTask>();
      if (!typed_task.IsNull()) {
        // Use HSHM strong copy method for actual copying
        typed_task->shm_strong_copy_main(*orig_task.Cast<FlushTask>());
        // Cast to base Task type for return
        dup_task = typed_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kClientSendTaskIn: {
      // Allocate new task using SHM default constructor
      auto typed_task = ipc_manager->NewTask<ClientSendTaskInTask>();
      if (!typed_task.IsNull()) {
        // Use HSHM strong copy method for actual copying
        typed_task->shm_strong_copy_main(*orig_task.Cast<ClientSendTaskInTask>());
        // Cast to base Task type for return
        dup_task = typed_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kServerRecvTaskIn: {
      // Allocate new task using SHM default constructor
      auto typed_task = ipc_manager->NewTask<ServerRecvTaskInTask>();
      if (!typed_task.IsNull()) {
        // Use HSHM strong copy method for actual copying
        typed_task->shm_strong_copy_main(*orig_task.Cast<ServerRecvTaskInTask>());
        // Cast to base Task type for return
        dup_task = typed_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kServerSendTaskOut: {
      // Allocate new task using SHM default constructor
      auto typed_task = ipc_manager->NewTask<ServerSendTaskOutTask>();
      if (!typed_task.IsNull()) {
        // Use HSHM strong copy method for actual copying
        typed_task->shm_strong_copy_main(*orig_task.Cast<ServerSendTaskOutTask>());
        // Cast to base Task type for return
        dup_task = typed_task.template Cast<chi::Task>();
      }
      break;
    }
    case Method::kClientRecvTaskOut: {
      // Allocate new task using SHM default constructor
      auto typed_task = ipc_manager->NewTask<ClientRecvTaskOutTask>();
      if (!typed_task.IsNull()) {
        // Use HSHM strong copy method for actual copying
        typed_task->shm_strong_copy_main(*orig_task.Cast<ClientRecvTaskOutTask>());
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

} // namespace chimaera::admin

#endif // ADMIN_AUTOGEN_LIB_EXEC_H_
