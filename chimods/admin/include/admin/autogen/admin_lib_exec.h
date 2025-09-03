#ifndef ADMIN_AUTOGEN_LIB_EXEC_H_
#define ADMIN_AUTOGEN_LIB_EXEC_H_

/**
 * Auto-generated execution dispatcher for Admin ChiMod
 * Provides switch-case dispatch for all implemented methods
 * Critical ChiMod for pool management and runtime control
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
    case Method::kGetOrCreatePool: {
      runtime->GetOrCreatePool(task.Cast<chimaera::admin::GetOrCreatePoolTask<chimaera::admin::CreateParams>>(), rctx);
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
 * Monitor a method on the runtime
 */
inline void Monitor(Runtime* runtime, chi::MonitorModeId mode, chi::u32 method, 
                   hipc::FullPtr<chi::Task> task_ptr, chi::RunContext& rctx) {
  switch (method) {
    case Method::kCreate: {
      runtime->MonitorCreate(mode, task_ptr.Cast<CreateTask>(), rctx);
      break;
    }
    case Method::kGetOrCreatePool: {
      runtime->MonitorGetOrCreatePool(mode, task_ptr.Cast<chimaera::admin::GetOrCreatePoolTask<chimaera::admin::CreateParams>>(), rctx);
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
    case Method::kGetOrCreatePool: {
      ipc_manager->DelTask(task_ptr.Cast<chimaera::admin::GetOrCreatePoolTask<chimaera::admin::CreateParams>>());
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

} // namespace chimaera::admin

#endif // ADMIN_AUTOGEN_LIB_EXEC_H_