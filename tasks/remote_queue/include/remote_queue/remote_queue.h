//
// Created by lukemartinlogan on 6/29/23.
//

#ifndef CHI_REMOTE_QUEUE_H_
#define CHI_REMOTE_QUEUE_H_

#include "remote_queue_tasks.h"

namespace chi::remote_queue {

/**
 * Create remote_queue requests
 *
 * This is ONLY used in the Hermes runtime, and
 * should never be called in client programs!!!
 * */
class Client : public ModuleClient {
 public:
  /** Default constructor */
  Client() = default;

  /** Destructor */
  ~Client() = default;

  /** Async create a pool */
  void Create(const DomainQuery &dom_query,
                  const DomainQuery &affinity,
                  const std::string &pool_name,
                  const CreateContext &ctx = CreateContext()) {
    LPointer<CreateTask> task = AsyncCreate(
        {}, dom_query, affinity, pool_name, ctx);
    task->SpinWait();
    Init(task->ctx_.id_);
    CHI_CLIENT->DelTask({}, task);
  }
  CHI_TASK_METHODS(Create);

  /** Destroy pool + queue */
  HSHM_ALWAYS_INLINE
  void Destroy(const DomainQuery &dom_query) {
    CHI_ADMIN->DestroyContainer(dom_query, id_);
  }

  /** Construct submit aggregator */
  CHI_TASK_METHODS(ClientPushSubmit)

  /** Construct submit aggregator */
  CHI_TASK_METHODS(ClientSubmit)

  /** Construct complete aggregator */
  CHI_TASK_METHODS(ServerComplete)
};

}  // namespace chi

#endif  // CHI_REMOTE_QUEUE_H_
