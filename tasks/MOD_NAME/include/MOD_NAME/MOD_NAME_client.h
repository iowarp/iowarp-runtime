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

#ifndef CHI_MOD_NAME_H_
#define CHI_MOD_NAME_H_

#include "MOD_NAME_tasks.h"

namespace chi::MOD_NAME {

/** Create MOD_NAME requests */
class Client : public ModuleClient {
public:
  /** Default constructor */
  HSHM_INLINE_CROSS_FUN
  Client() = default;

  /** Destructor */
  HSHM_INLINE_CROSS_FUN
  ~Client() = default;

  CHI_BEGIN(Create)
  /** Create a pool */
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
  CHI_END(Create)

  CHI_BEGIN(Destroy)
  /** Destroy pool + queue */
  HSHM_INLINE_CROSS_FUN
  void Destroy(const hipc::MemContext &mctx, const DomainQuery &dom_query) {
    FullPtr<DestroyTask> task = AsyncDestroy(mctx, dom_query, pool_id_);
    task->Wait();
    CHI_CLIENT->DelTask(mctx, task);
  }
  CHI_TASK_METHODS(Destroy)
  CHI_END(Destroy)

  CHI_AUTOGEN_METHODS // keep at class bottom
};

} // namespace chi::MOD_NAME

#endif // CHI_MOD_NAME_H_
