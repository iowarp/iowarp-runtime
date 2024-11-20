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

#ifndef CHI_TASK_NAME_H_
#define CHI_TASK_NAME_H_

#include "TASK_NAME_tasks.h"

namespace chi::TASK_NAME {

/** Create TASK_NAME requests */
class Client : public ModuleClient {

 public:
  /** Default constructor */
  Client() = default;

  /** Destructor */
  ~Client() = default;

  /** Create a pool */
  void Create(const hipc::MemContext &mctx,
              const DomainQuery &dom_query,
              const DomainQuery &affinity,
              const std::string &pool_name,
              const CreateContext &ctx = CreateContext()) {
    LPointer<CreateTask> task = AsyncCreate(
        mctx, dom_query, affinity, pool_name, ctx);
    task->Wait();
    Init(task->ctx_.id_);
    CHI_CLIENT->DelTask(mctx, task);
  }
  CHI_TASK_METHODS(Create);

  /** Destroy pool + queue */
  HSHM_ALWAYS_INLINE
  void Destroy(const hipc::MemContext &mctx,
               const DomainQuery &dom_query) {
    CHI_ADMIN->DestroyContainer(mctx, dom_query, id_);
  }

  /** Call a custom method */
  HSHM_ALWAYS_INLINE
  void Custom(const hipc::MemContext &mctx,
              const DomainQuery &dom_query) {
    LPointer<CustomTask> task = AsyncCustom(mctx, dom_query);
    task.ptr_->Wait();
  }
  CHI_TASK_METHODS(Custom);
};

}  // namespace chi

#endif  // CHI_TASK_NAME_H_
