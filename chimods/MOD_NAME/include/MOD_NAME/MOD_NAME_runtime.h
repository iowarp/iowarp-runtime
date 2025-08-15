#ifndef MOD_NAME_RUNTIME_H_
#define MOD_NAME_RUNTIME_H_

#include <chimaera/chimaera.h>
#include <chimaera/container.h>
#include "MOD_NAME_tasks.h"
#include "autogen/MOD_NAME_methods.h"
#include <memory>

namespace chimaera::MOD_NAME {

// Forward declarations (CustomTask only, CreateTask is a using alias in MOD_NAME_tasks.h)
struct CustomTask;

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
};

} // namespace chimaera::MOD_NAME

#endif // MOD_NAME_RUNTIME_H_