/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Hermes. The full Hermes copyright notice, including  *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the top directory. If you do not  *
 * have access to the file, you may request a copy from help@hdfgroup.org.   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef HRUN_bdev_H_
#define HRUN_bdev_H_

#include "bdev_tasks.h"

namespace chi::bdev {

/** Create bdev requests */
class Client : public TaskLibClient {

 public:
  /** Default constructor */
  Client() = default;

  /** Destructor */
  ~Client() = default;

  /** Create a task state */
  void AsyncCreateConstruct(CreateTask *task,
                            const TaskNode &task_node,
                            const DomainQuery &dom_query,
                            const DomainQuery &affinity,
                            const std::string &pool_name,
                            const CreateContext &ctx,
                            const std::string &path,
                            size_t max_size) {
    CHI_CLIENT->ConstructTask<CreateTask>(
        task, task_node, dom_query, affinity, pool_name, ctx,
        path, max_size);
  }
  void CreateRoot(const DomainQuery &dom_query,
                  const DomainQuery &affinity,
                  const std::string &pool_name,
                  const std::string &path,
                  size_t max_size,
                  const CreateContext &ctx = CreateContext()) {
    LPointer<CreateTask> task = AsyncCreateRoot(
        dom_query, affinity, pool_name, ctx, path, max_size);
    task->Wait();
    Init(task->ctx_.id_);
    CHI_CLIENT->DelTask(task);
  }
  CHI_TASK_METHODS(Create);

  /** Destroy task state + queue */
  HSHM_ALWAYS_INLINE
  void DestroyRoot(const DomainQuery &dom_query) {
    CHI_ADMIN->DestroyContainerRoot(dom_query, id_);
  }

  /** Call a custom method */
  HSHM_ALWAYS_INLINE
  void AsyncWriteConstruct(WriteTask *task,
                           const TaskNode &task_node,
                           const DomainQuery &dom_query,
                           const hipc::Pointer &data,
                           size_t size,
                           size_t off) {
    CHI_CLIENT->ConstructTask<WriteTask>(
        task, task_node, dom_query, id_,
        data, size, off);
  }
  HSHM_ALWAYS_INLINE
  void WriteRoot(const DomainQuery &dom_query,
                 const hipc::Pointer &data,
                 size_t size,
                 size_t off) {
    LPointer<WriteTask> task = AsyncWriteRoot(dom_query, data, size, off);
    task.ptr_->Wait();
  }
  CHI_TASK_METHODS(Write);
};

}  // namespace chi

#endif  // HRUN_bdev_H_
