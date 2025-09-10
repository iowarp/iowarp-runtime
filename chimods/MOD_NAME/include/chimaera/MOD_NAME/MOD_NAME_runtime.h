#ifndef MOD_NAME_RUNTIME_H_
#define MOD_NAME_RUNTIME_H_

#include <chimaera/chimaera.h>
#include <chimaera/container.h>
#include <chimaera/comutex.h>
#include <chimaera/corwlock.h>
#include "MOD_NAME_tasks.h"
#include "autogen/MOD_NAME_methods.h"
#include "MOD_NAME_client.h"

namespace chimaera::MOD_NAME {

// Forward declarations (CustomTask only, CreateTask is a using alias in MOD_NAME_tasks.h)
struct CustomTask;
struct CoMutexTestTask;
struct CoRwLockTestTask;
struct FireAndForgetTestTask;

/**
 * Runtime implementation for MOD_NAME container
 */
class Runtime : public chi::Container {
public:
  // CreateParams type used by CHI_TASK_CC macro for lib_name access
  using CreateParams = chimaera::MOD_NAME::CreateParams;

private:
  // Container-specific state
  chi::u32 create_count_ = 0;
  chi::u32 custom_count_ = 0;

  // Client for making calls to this ChiMod
  Client client_;

  // Static synchronization objects for testing
  static chi::CoMutex test_comutex_;
  static chi::CoRwLock test_corwlock_;

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
   * Handle Create task
   */
  void Create(hipc::FullPtr<CreateTask> task, chi::RunContext& rctx);

  /**
   * Monitor Create task
   */
  void MonitorCreate(chi::MonitorModeId mode, 
                    hipc::FullPtr<CreateTask> task_ptr,
                    chi::RunContext& rctx);

  /**
   * Handle Custom task
   */
  void Custom(hipc::FullPtr<CustomTask> task, chi::RunContext& rctx);

  /**
   * Monitor Custom task
   */
  void MonitorCustom(chi::MonitorModeId mode, 
                    hipc::FullPtr<CustomTask> task_ptr,
                    chi::RunContext& rctx);

  /**
   * Handle CoMutexTest task
   */
  void CoMutexTest(hipc::FullPtr<CoMutexTestTask> task, chi::RunContext& rctx);

  /**
   * Monitor CoMutexTest task
   */
  void MonitorCoMutexTest(chi::MonitorModeId mode, 
                         hipc::FullPtr<CoMutexTestTask> task_ptr,
                         chi::RunContext& rctx);

  /**
   * Handle CoRwLockTest task
   */
  void CoRwLockTest(hipc::FullPtr<CoRwLockTestTask> task, chi::RunContext& rctx);

  /**
   * Monitor CoRwLockTest task
   */
  void MonitorCoRwLockTest(chi::MonitorModeId mode, 
                          hipc::FullPtr<CoRwLockTestTask> task_ptr,
                          chi::RunContext& rctx);

  /**
   * Handle FireAndForgetTest task
   */
  void FireAndForgetTest(hipc::FullPtr<FireAndForgetTestTask> task, chi::RunContext& rctx);

  /**
   * Monitor FireAndForgetTest task
   */
  void MonitorFireAndForgetTest(chi::MonitorModeId mode, 
                               hipc::FullPtr<FireAndForgetTestTask> task_ptr,
                               chi::RunContext& rctx);

  /**
   * Handle Destroy task - Alias for DestroyPool (DestroyTask = DestroyPoolTask)
   */
  void Destroy(hipc::FullPtr<DestroyTask> task, chi::RunContext& rctx);

  /**
   * Monitor Destroy task - Alias for MonitorDestroyPool (DestroyTask = DestroyPoolTask)
   */
  void MonitorDestroy(chi::MonitorModeId mode, 
                     hipc::FullPtr<DestroyTask> task_ptr,
                     chi::RunContext& rctx);

  /**
   * Get remaining work count for this container
   * Template implementation returns 0 (no work tracking)
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

} // namespace chimaera::MOD_NAME

#endif // MOD_NAME_RUNTIME_H_