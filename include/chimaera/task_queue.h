#ifndef CHIMAERA_INCLUDE_CHIMAERA_TASK_QUEUE_H_
#define CHIMAERA_INCLUDE_CHIMAERA_TASK_QUEUE_H_

#include "chimaera/types.h"

namespace chi {

/**
 * Header for TaskQueue containing PoolId and worker id
 */
struct TaskQueueHeader {
  PoolId pool_id;
  WorkerId assigned_worker_id;
  
  TaskQueueHeader() : pool_id(0), assigned_worker_id(0) {}
  TaskQueueHeader(PoolId pid, WorkerId wid = 0) : pool_id(pid), assigned_worker_id(wid) {}
};

/**
 * TaskQueue typedef - hipc::multi_mpsc_queue for hipc::Pointer data
 */
using TaskQueue = hipc::multi_mpsc_queue<hipc::Pointer>;

} // namespace chi

#endif // CHIMAERA_INCLUDE_CHIMAERA_TASK_QUEUE_H_