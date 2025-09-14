#ifndef ADMIN_CLIENT_H_
#define ADMIN_CLIENT_H_

#include <chimaera/chimaera.h>

#include "admin_tasks.h"

/**
 * Client API for Admin ChiMod
 *
 * Critical ChiMod for managing ChiPools and runtime lifecycle.
 * Provides methods for external programs to create/destroy pools and stop
 * runtime.
 */

namespace chimaera::admin {

class Client : public chi::ContainerClient {
 public:
  /**
   * Default constructor
   */
  Client() = default;

  /**
   * Constructor with pool ID
   */
  explicit Client(const chi::PoolId& pool_id) { Init(pool_id); }

  /**
   * Create the Admin container (synchronous)
   */
  void Create(const hipc::MemContext& mctx, const chi::PoolQuery& pool_query) {
    auto task = AsyncCreate(mctx, pool_query);
    task->Wait();

    // CRITICAL: Update client pool_id_ with the actual pool ID from the task
    pool_id_ = task->new_pool_id_;

    // Clean up task
    auto* ipc_manager = CHI_IPC;
    ipc_manager->DelTask(task);
  }

  /**
   * Create the Admin container (asynchronous)
   */
  hipc::FullPtr<CreateTask> AsyncCreate(const hipc::MemContext& mctx,
                                        const chi::PoolQuery& pool_query) {
    auto* ipc_manager = CHI_IPC;

    // Allocate CreateTask for admin container creation
    auto task = ipc_manager->NewTask<CreateTask>(chi::CreateTaskNode(),
                                                 pool_id_, pool_query);

    // Submit to runtime
    ipc_manager->Enqueue(task);

    return task;
  }

  /**
   * Destroy an existing ChiPool (synchronous)
   */
  void DestroyPool(const hipc::MemContext& mctx,
                   const chi::PoolQuery& pool_query, chi::PoolId target_pool_id,
                   chi::u32 destruction_flags = 0) {
    auto task =
        AsyncDestroyPool(mctx, pool_query, target_pool_id, destruction_flags);
    task->Wait();

    // Check for errors
    if (task->return_code_ != 0) {
      std::string error = task->error_message_.str();
      auto* ipc_manager = CHI_IPC;
      ipc_manager->DelTask(task);
      throw std::runtime_error("Pool destruction failed: " + error);
    }

    // Clean up task
    auto* ipc_manager = CHI_IPC;
    ipc_manager->DelTask(task);
  }

  /**
   * Destroy an existing ChiPool (asynchronous)
   */
  hipc::FullPtr<DestroyPoolTask> AsyncDestroyPool(
      const hipc::MemContext& mctx, const chi::PoolQuery& pool_query,
      chi::PoolId target_pool_id, chi::u32 destruction_flags = 0) {
    auto* ipc_manager = CHI_IPC;

    // Allocate DestroyPoolTask
    auto task = ipc_manager->NewTask<DestroyPoolTask>(
        chi::CreateTaskNode(), pool_id_, pool_query, target_pool_id,
        destruction_flags);

    // Submit to runtime
    ipc_manager->Enqueue(task);

    return task;
  }

  /**
   * Send task to remote node (synchronous)
   */
  template <typename TaskType>
  void ClientSendTaskIn(const hipc::MemContext& mctx,
                        const std::vector<chi::PoolQuery>& pool_queries,
                        const hipc::FullPtr<TaskType>& task_to_send) {
    auto task = AsyncClientSendTaskIn(mctx, pool_queries, task_to_send);
    task->Wait();

    // Check for errors
    if (task->return_code_ != 0) {
      std::string error = task->error_message_.str();
      auto* ipc_manager = CHI_IPC;
      ipc_manager->DelTask(task);
      throw std::runtime_error("Task send failed: " + error);
    }

    // Clean up task
    auto* ipc_manager = CHI_IPC;
    ipc_manager->DelTask(task);
  }

  /**
   * Send task to remote node (asynchronous)
   */
  template <typename TaskType>
  hipc::FullPtr<ClientSendTaskInTask> AsyncClientSendTaskIn(
      const hipc::MemContext& mctx,
      const std::vector<chi::PoolQuery>& pool_queries,
      const hipc::FullPtr<TaskType>& task_to_send) {
    auto* ipc_manager = CHI_IPC;

    // Use local routing
    chi::PoolQuery local_pool_query = chi::PoolQuery::Local();

    // Allocate ClientSendTaskInTask with pool queries and task
    auto task = ipc_manager->NewTask<ClientSendTaskInTask>(
        chi::CreateTaskNode(), pool_id_, local_pool_query, pool_queries,
        task_to_send, 0);

    // Submit to runtime
    ipc_manager->Enqueue(task);

    return task;
  }

  /**
   * Poll and receive tasks from network (synchronous)
   * Periodic task that deserializes incoming tasks from remote nodes
   */
  void ServerRecvTaskIn(const hipc::MemContext& mctx,
                        const chi::PoolQuery& pool_query) {
    auto task = AsyncServerRecvTaskIn(mctx, pool_query);
    task->Wait();

    // Check for errors
    if (task->return_code_ != 0) {
      std::string error = task->error_message_.str();
      auto* ipc_manager = CHI_IPC;
      ipc_manager->DelTask(task);
      throw std::runtime_error("Task receive failed: " + error);
    }

    // Clean up task
    auto* ipc_manager = CHI_IPC;
    ipc_manager->DelTask(task);
  }

  /**
   * Poll and receive tasks from network (asynchronous)
   * Periodic task that deserializes incoming tasks from remote nodes
   */
  hipc::FullPtr<ServerRecvTaskInTask> AsyncServerRecvTaskIn(
      const hipc::MemContext& mctx, const chi::PoolQuery& pool_query) {
    auto* ipc_manager = CHI_IPC;

    // Allocate ServerRecvTaskInTask for periodic polling
    auto task = ipc_manager->NewTask<ServerRecvTaskInTask>(
        chi::CreateTaskNode(), pool_id_, pool_query, 0);

    // Submit to runtime
    ipc_manager->Enqueue(task);

    return task;
  }

  /**
   * Send task output to remote node (synchronous)
   */
  template <typename TaskType>
  void ServerSendTaskOut(const hipc::MemContext& mctx,
                         const chi::PoolQuery& pool_query,
                         chi::u32 target_node_id,
                         const hipc::FullPtr<TaskType>& completed_task) {
    auto task = AsyncServerSendTaskOut(mctx, pool_query, target_node_id,
                                       completed_task);
    task->Wait();

    // Check for errors
    if (task->return_code_ != 0) {
      std::string error = task->error_message_.str();
      auto* ipc_manager = CHI_IPC;
      ipc_manager->DelTask(task);
      throw std::runtime_error("Task output send failed: " + error);
    }

    // Clean up task
    auto* ipc_manager = CHI_IPC;
    ipc_manager->DelTask(task);
  }

  /**
   * Send task output to remote node (asynchronous)
   */
  template <typename TaskType>
  hipc::FullPtr<ServerSendTaskOutTask> AsyncServerSendTaskOut(
      const hipc::MemContext& mctx, const chi::PoolQuery& pool_query,
      chi::u32 target_node_id, const hipc::FullPtr<TaskType>& completed_task) {
    auto* ipc_manager = CHI_IPC;

    // Allocate ServerSendTaskOutTask with the original completed task (no
    // serialization)
    auto task = ipc_manager->NewTask<ServerSendTaskOutTask>(
        chi::CreateTaskNode(), pool_id_, pool_query,
        static_cast<hipc::FullPtr<chi::Task>>(completed_task), 0);

    // Submit to runtime
    ipc_manager->Enqueue(task);

    return task;
  }

  /**
   * Poll and receive task outputs from network (synchronous)
   * Periodic task that deserializes incoming task results from remote nodes
   */
  void ClientRecvTaskOut(const hipc::MemContext& mctx,
                         const chi::PoolQuery& pool_query) {
    auto task = AsyncClientRecvTaskOut(mctx, pool_query);
    task->Wait();

    // Check for errors
    if (task->return_code_ != 0) {
      std::string error = task->error_message_.str();
      auto* ipc_manager = CHI_IPC;
      ipc_manager->DelTask(task);
      throw std::runtime_error("Task output receive failed: " + error);
    }

    // Clean up task
    auto* ipc_manager = CHI_IPC;
    ipc_manager->DelTask(task);
  }

  /**
   * Poll and receive task outputs from network (asynchronous)
   * Periodic task that deserializes incoming task results from remote nodes
   */
  hipc::FullPtr<ClientRecvTaskOutTask> AsyncClientRecvTaskOut(
      const hipc::MemContext& mctx, const chi::PoolQuery& pool_query) {
    auto* ipc_manager = CHI_IPC;

    // Allocate ClientRecvTaskOutTask for periodic polling
    auto task = ipc_manager->NewTask<ClientRecvTaskOutTask>(
        chi::CreateTaskNode(), pool_id_, pool_query, 0);

    // Submit to runtime
    ipc_manager->Enqueue(task);

    return task;
  }

  /**
   * Flush administrative operations (synchronous)
   */
  void Flush(const hipc::MemContext& mctx, const chi::PoolQuery& pool_query) {
    auto task = AsyncFlush(mctx, pool_query);
    task->Wait();

    // Check for errors
    if (task->return_code_ != 0) {
      auto* ipc_manager = CHI_IPC;
      ipc_manager->DelTask(task);
      throw std::runtime_error("Flush failed with result code: " +
                               std::to_string(task->return_code_));
    }

    // Clean up task
    auto* ipc_manager = CHI_IPC;
    ipc_manager->DelTask(task);
  }

  /**
   * Flush administrative operations (asynchronous)
   */
  hipc::FullPtr<FlushTask> AsyncFlush(const hipc::MemContext& mctx,
                                      const chi::PoolQuery& pool_query) {
    auto* ipc_manager = CHI_IPC;

    // Allocate FlushTask
    auto task = ipc_manager->NewTask<FlushTask>(chi::CreateTaskNode(), pool_id_,
                                                pool_query);

    // Submit to runtime
    ipc_manager->Enqueue(task);

    return task;
  }

  /**
   * Stop the entire Chimaera runtime (asynchronous)
   */
  hipc::FullPtr<StopRuntimeTask> AsyncStopRuntime(
      const hipc::MemContext& mctx, const chi::PoolQuery& pool_query,
      chi::u32 shutdown_flags = 0, chi::u32 grace_period_ms = 5000) {
    auto* ipc_manager = CHI_IPC;

    // Allocate StopRuntimeTask
    auto task = ipc_manager->NewTask<StopRuntimeTask>(
        chi::CreateTaskNode(), pool_id_, pool_query, shutdown_flags,
        grace_period_ms);

    // Submit to runtime
    ipc_manager->Enqueue(task);

    return task;
  }
};

}  // namespace chimaera::admin

#endif  // ADMIN_CLIENT_H_