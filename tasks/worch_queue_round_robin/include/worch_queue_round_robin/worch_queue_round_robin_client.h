//
// Created by lukemartinlogan on 6/29/23.
//

#ifndef CHI_worch_queue_round_robin_H_
#define CHI_worch_queue_round_robin_H_

#include "worch_queue_round_robin_tasks.h"

namespace chi::worch_queue_round_robin {

/** Create admin requests */
class Client : public ModuleClient {
 public:
  /** Default constructor */
  HSHM_INLINE_CROSS_FUN
  Client() = default;

  /** Destructor */
  HSHM_INLINE_CROSS_FUN
  ~Client() = default;

  /** Create a worch_queue_round_robin */
  HSHM_INLINE_CROSS_FUN
  void Create(const hipc::MemContext &mctx, const DomainQuery &dom_query,
              const DomainQuery &affinity, const chi::string &pool_name,
              const CreateContext &ctx = CreateContext()) {
    FullPtr<CreateTask> task =
        AsyncCreate(mctx, dom_query, affinity, pool_name, ctx);
    task->Wait();
    Init(task->ctx_.id_);
    CHI_CLIENT->DelTask(mctx, task);
  }
  CHI_TASK_METHODS(Create);

  /** Destroy pool */
  HSHM_INLINE_CROSS_FUN
  void Destroy(const hipc::MemContext &mctx, const DomainQuery &dom_query) {
    CHI_ADMIN->DestroyContainer(mctx, dom_query, id_);
  }

  CHI_AUTOGEN_METHODS  // keep at class bottom
};

}  // namespace chi::worch_queue_round_robin

#endif  // CHI_worch_queue_round_robin_H_
