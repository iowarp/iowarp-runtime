//
// Created by lukemartinlogan on 6/29/23.
//

#ifndef CHI_worch_proc_round_robin_H_
#define CHI_worch_proc_round_robin_H_

#include "worch_proc_round_robin_tasks.h"

namespace chi::worch_proc_round_robin {

/** Create admin requests */
class Client : public ModuleClient {

 public:
  /** Default constructor */
  Client() = default;

  /** Destructor */
  ~Client() = default;

  /** Create a worch_proc_round_robin */
  void Create(const hipc::MemContext &mctx,
              const DomainQuery &dom_query,
              const DomainQuery &affinity,
              const std::string &pool_name,
              const CreateContext &ctx = CreateContext()) {
    FullPtr<CreateTask> task = AsyncCreate(
        mctx, dom_query, affinity, pool_name, ctx);
    task->Wait();
    Init(task->ctx_.id_);
    CHI_CLIENT->DelTask(mctx, task);
  }
  CHI_TASK_METHODS(Create);

  /** Destroy state */
  HSHM_INLINE
  void Destroy(const hipc::MemContext &mctx,
               const DomainQuery &dom_query) {
    CHI_ADMIN->DestroyContainer(mctx, dom_query, id_);
  }
};

}  // namespace chi

#endif  // CHI_worch_proc_round_robin_H_
