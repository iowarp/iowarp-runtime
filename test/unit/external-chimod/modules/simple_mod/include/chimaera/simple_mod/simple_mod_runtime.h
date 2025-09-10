#ifndef SIMPLE_MOD_RUNTIME_H_
#define SIMPLE_MOD_RUNTIME_H_

#include <chimaera/chimaera.h>
#include <chimaera/container.h>
#include "simple_mod_tasks.h"
#include "simple_mod_client.h"

namespace external_test::simple_mod {

// Simple mod local queue indices
enum SimpleModQueueIndex {
  kMetadataQueue = 0,  // Queue for metadata operations
};

/**
 * Runtime implementation for Simple Mod container
 * 
 * Minimal ChiMod for testing external development patterns.
 * Demonstrates basic runtime structure for external ChiMod development.
 */
class Runtime : public chi::Container {
public:
  // CreateParams type used by CHI_TASK_CC macro for lib_name access
  using CreateParams = external_test::simple_mod::CreateParams;

private:
  // Container-specific state
  chi::u32 create_count_ = 0;
  
  // Client for making calls to this ChiMod
  Client client_;

public:
  /**
   * Constructor
   */
  Runtime() = default;

  /**
   * Destructor
   */
  virtual ~Runtime() = default;

  /**
   * Initialize client for this container
   */
  void InitClient(const chi::PoolId& pool_id) override;

  /**
   * Execute a method on a task
   */
  void Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr, chi::RunContext& rctx) override;

  /**
   * Monitor a method execution for scheduling/coordination
   */
  void Monitor(chi::MonitorModeId mode, chi::u32 method, 
              hipc::FullPtr<chi::Task> task_ptr,
              chi::RunContext& rctx) override;

  /**
   * Delete/cleanup a task
   */
  void Del(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) override;

  //===========================================================================
  // Method implementations
  //===========================================================================

  /**
   * Handle Create task - Initialize the Simple Mod container
   */
  void Create(hipc::FullPtr<CreateTask> task, chi::RunContext& rctx);

  /**
   * Monitor Create task
   */
  void MonitorCreate(chi::MonitorModeId mode, 
                    hipc::FullPtr<CreateTask> task_ptr,
                    chi::RunContext& rctx);

  /**
   * Handle Destroy task - Destroy the Simple Mod container
   */
  void Destroy(hipc::FullPtr<DestroyTask> task, chi::RunContext& rctx);

  /**
   * Monitor Destroy task
   */
  void MonitorDestroy(chi::MonitorModeId mode, 
                     hipc::FullPtr<DestroyTask> task_ptr,
                     chi::RunContext& rctx);

  /**
   * Handle Flush task - Flush simple mod operations
   */
  void Flush(hipc::FullPtr<FlushTask> task, chi::RunContext& rctx);

  /**
   * Monitor Flush task
   */
  void MonitorFlush(chi::MonitorModeId mode, 
                   hipc::FullPtr<FlushTask> task_ptr,
                   chi::RunContext& rctx);

  /**
   * Get remaining work count for this simple mod container
   */
  chi::u64 GetWorkRemaining() const override;

  //===========================================================================
  // Task Serialization Methods
  //===========================================================================

  /**
   * Serialize task IN parameters for network transfer
   */
  void SaveIn(chi::u32 method, chi::TaskSaveInArchive& archive, hipc::FullPtr<chi::Task> task_ptr) override;

  /**
   * Deserialize task IN parameters from network transfer
   */
  void LoadIn(chi::u32 method, chi::TaskLoadInArchive& archive, hipc::FullPtr<chi::Task> task_ptr) override;

  /**
   * Serialize task OUT parameters for network transfer
   */
  void SaveOut(chi::u32 method, chi::TaskSaveOutArchive& archive, hipc::FullPtr<chi::Task> task_ptr) override;

  /**
   * Deserialize task OUT parameters from network transfer
   */
  void LoadOut(chi::u32 method, chi::TaskLoadOutArchive& archive, hipc::FullPtr<chi::Task> task_ptr) override;

  /**
   * Create a new copy of a task (deep copy for distributed execution)
   */
  void NewCopy(chi::u32 method, 
               const hipc::FullPtr<chi::Task> &orig_task,
               hipc::FullPtr<chi::Task> &dup_task, bool deep) override;
};

}  // namespace external_test::simple_mod

#endif  // SIMPLE_MOD_RUNTIME_H_