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

#ifndef CHI_bdev_H_
#define CHI_bdev_H_

#include "bdev_tasks.h"

namespace chi::bdev {

/** Create bdev requests */
class Client : public ModuleClient {
 public:
  /** Default constructor */
  Client() = default;

  /** Destructor */
  ~Client() = default;

  /** Create a pool */
  HSHM_INLINE_CROSS_FUN
  void Create(const hipc::MemContext &mctx, const DomainQuery &dom_query,
              const DomainQuery &affinity, const chi::string &pool_name,
              const chi::string &path, size_t max_size,
              const CreateContext &ctx = CreateContext()) {
    FullPtr<CreateTask> task =
        AsyncCreate(mctx, dom_query, affinity, pool_name, ctx, path, max_size);
    task->Wait();
    Init(task->ctx_.id_);
    CHI_CLIENT->DelTask(mctx, task);
  }
  CHI_TASK_METHODS(Create);

  /** Destroy pool + queue */
  HSHM_INLINE_CROSS_FUN
  void Destroy(const hipc::MemContext &mctx, const DomainQuery &dom_query) {
    CHI_ADMIN->DestroyContainer(mctx, dom_query, id_);
  }

  /** Allocate a section of the block device */
  HSHM_INLINE_CROSS_FUN
  std::vector<Block> Allocate(const hipc::MemContext &mctx,
                              const DomainQuery &dom_query, size_t size) {
    FullPtr<AllocateTask> task = AsyncAllocate(mctx, dom_query, size);
    task.ptr_->Wait();
    std::vector<Block> blocks = task->blocks_.vec();
    CHI_CLIENT->DelTask(mctx, task);
    return blocks;
  }
  CHI_TASK_METHODS(Allocate);

  /** Free a section of the block device */
  HSHM_INLINE_CROSS_FUN
  void Free(const hipc::MemContext &mctx, const DomainQuery &dom_query,
            const Block &block) {
    FullPtr<FreeTask> task = AsyncFree(mctx, dom_query, block);
    task.ptr_->Wait();
    CHI_CLIENT->DelTask(mctx, task);
  }
  CHI_TASK_METHODS(Free);

  /** Write to the block device */
  HSHM_INLINE_CROSS_FUN
  void Write(const hipc::MemContext &mctx, const DomainQuery &dom_query,
             const hipc::Pointer &data, size_t off, size_t size) {
    FullPtr<WriteTask> task = AsyncWrite(mctx, dom_query, data, off, size);
    task.ptr_->Wait();
    CHI_CLIENT->DelTask(mctx, task);
  }
  CHI_TASK_METHODS(Write);

  /** Read from the block device */
  HSHM_INLINE_CROSS_FUN
  void Read(const hipc::MemContext &mctx, const DomainQuery &dom_query,
            const hipc::Pointer &data, size_t size, size_t off) {
    FullPtr<ReadTask> task = AsyncRead(mctx, dom_query, data, size, off);
    task.ptr_->Wait();
    CHI_CLIENT->DelTask(mctx, task);
  }
  CHI_TASK_METHODS(Read);

  /** Periodically poll block device stats */
  HSHM_INLINE_CROSS_FUN
  BdevStats PollStats(const hipc::MemContext &mctx,
                      const DomainQuery &dom_query) {
    FullPtr<PollStatsTask> task = AsyncPollStats(mctx, dom_query, 0);
    task.ptr_->Wait();
    BdevStats stats = task->stats_;
    CHI_CLIENT->DelTask(mctx, task);
    return stats;
  }
  CHI_TASK_METHODS(PollStats);

  CHI_AUTOGEN_METHODS  // keep at class bottom
};

}  // namespace chi::bdev

#endif  // CHI_bdev_H_
