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
      runtime->SaveIn(Method::kCreate, archive, task_ptr);
      break;
    }
    case Method::kDestroy: {
      runtime->SaveIn(Method::kDestroy, archive, task_ptr);
      break;
    }
    case Method::kGetOrCreatePool: {
      runtime->SaveIn(Method::kGetOrCreatePool, archive, task_ptr);
      break;
    }
    case Method::kDestroyPool: {
      runtime->SaveIn(Method::kDestroyPool, archive, task_ptr);
      break;
    }
    case Method::kStopRuntime: {
      runtime->SaveIn(Method::kStopRuntime, archive, task_ptr);
      break;
    }
    case Method::kFlush: {
      runtime->SaveIn(Method::kFlush, archive, task_ptr);
      break;
    }
    case Method::kClientSendTaskIn: {
      runtime->SaveIn(Method::kClientSendTaskIn, archive, task_ptr);
      break;
    }
    case Method::kServerRecvTaskIn: {
      runtime->SaveIn(Method::kServerRecvTaskIn, archive, task_ptr);
      break;
    }
    case Method::kServerSendTaskOut: {
      runtime->SaveIn(Method::kServerSendTaskOut, archive, task_ptr);
      break;
    }
    case Method::kClientRecvTaskOut: {
      runtime->SaveIn(Method::kClientRecvTaskOut, archive, task_ptr);
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
      runtime->LoadIn(Method::kCreate, archive, task_ptr);
      break;
    }
    case Method::kDestroy: {
      runtime->LoadIn(Method::kDestroy, archive, task_ptr);
      break;
    }
    case Method::kGetOrCreatePool: {
      runtime->LoadIn(Method::kGetOrCreatePool, archive, task_ptr);
      break;
    }
    case Method::kDestroyPool: {
      runtime->LoadIn(Method::kDestroyPool, archive, task_ptr);
      break;
    }
    case Method::kStopRuntime: {
      runtime->LoadIn(Method::kStopRuntime, archive, task_ptr);
      break;
    }
    case Method::kFlush: {
      runtime->LoadIn(Method::kFlush, archive, task_ptr);
      break;
    }
    case Method::kClientSendTaskIn: {
      runtime->LoadIn(Method::kClientSendTaskIn, archive, task_ptr);
      break;
    }
    case Method::kServerRecvTaskIn: {
      runtime->LoadIn(Method::kServerRecvTaskIn, archive, task_ptr);
      break;
    }
    case Method::kServerSendTaskOut: {
      runtime->LoadIn(Method::kServerSendTaskOut, archive, task_ptr);
      break;
    }
    case Method::kClientRecvTaskOut: {
      runtime->LoadIn(Method::kClientRecvTaskOut, archive, task_ptr);
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
      runtime->SaveOut(Method::kCreate, archive, task_ptr);
      break;
    }
    case Method::kDestroy: {
      runtime->SaveOut(Method::kDestroy, archive, task_ptr);
      break;
    }
    case Method::kGetOrCreatePool: {
      runtime->SaveOut(Method::kGetOrCreatePool, archive, task_ptr);
      break;
    }
    case Method::kDestroyPool: {
      runtime->SaveOut(Method::kDestroyPool, archive, task_ptr);
      break;
    }
    case Method::kStopRuntime: {
      runtime->SaveOut(Method::kStopRuntime, archive, task_ptr);
      break;
    }
    case Method::kFlush: {
      runtime->SaveOut(Method::kFlush, archive, task_ptr);
      break;
    }
    case Method::kClientSendTaskIn: {
      runtime->SaveOut(Method::kClientSendTaskIn, archive, task_ptr);
      break;
    }
    case Method::kServerRecvTaskIn: {
      runtime->SaveOut(Method::kServerRecvTaskIn, archive, task_ptr);
      break;
    }
    case Method::kServerSendTaskOut: {
      runtime->SaveOut(Method::kServerSendTaskOut, archive, task_ptr);
      break;
    }
    case Method::kClientRecvTaskOut: {
      runtime->SaveOut(Method::kClientRecvTaskOut, archive, task_ptr);
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
      runtime->LoadOut(Method::kCreate, archive, task_ptr);
      break;
    }
    case Method::kDestroy: {
      runtime->LoadOut(Method::kDestroy, archive, task_ptr);
      break;
    }
    case Method::kGetOrCreatePool: {
      runtime->LoadOut(Method::kGetOrCreatePool, archive, task_ptr);
      break;
    }
    case Method::kDestroyPool: {
      runtime->LoadOut(Method::kDestroyPool, archive, task_ptr);
      break;
    }
    case Method::kStopRuntime: {
      runtime->LoadOut(Method::kStopRuntime, archive, task_ptr);
      break;
    }
    case Method::kFlush: {
      runtime->LoadOut(Method::kFlush, archive, task_ptr);
      break;
    }
    case Method::kClientSendTaskIn: {
      runtime->LoadOut(Method::kClientSendTaskIn, archive, task_ptr);
      break;
    }
    case Method::kServerRecvTaskIn: {
      runtime->LoadOut(Method::kServerRecvTaskIn, archive, task_ptr);
      break;
    }
    case Method::kServerSendTaskOut: {
      runtime->LoadOut(Method::kServerSendTaskOut, archive, task_ptr);
      break;
    }
    case Method::kClientRecvTaskOut: {
      runtime->LoadOut(Method::kClientRecvTaskOut, archive, task_ptr);
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

} // namespace chimaera::admin

#endif // ADMIN_AUTOGEN_LIB_EXEC_H_
