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
  void Create(const DomainQuery &dom_query,
              const DomainQuery &affinity,
              const std::string &pool_name,
              const std::string &path,
              size_t max_size,
              const CreateContext &ctx = CreateContext()) {
    LPointer<CreateTask> task = AsyncCreate(
        dom_query, affinity, pool_name, ctx, path, max_size);
    task->Wait();
    Init(task->ctx_.id_);
    CHI_CLIENT->DelTask(task);
  }
  CHI_TASK_METHODS(Create);

  /** Destroy pool + queue */
  HSHM_ALWAYS_INLINE
  void Destroy(const DomainQuery &dom_query) {
    CHI_ADMIN->DestroyContainer(dom_query, id_);
  }

  /** Allocate a section of the block device */
  HSHM_ALWAYS_INLINE
  void AsyncAllocateConstruct(AllocateTask *task,
                              const TaskNode &task_node,
                              const DomainQuery &dom_query,
                              size_t size) {
    CHI_CLIENT->ConstructTask<AllocateTask>(
        task, task_node, dom_query, id_, size);
  }
  HSHM_ALWAYS_INLINE
  std::vector<Block> Allocate(const DomainQuery &dom_query,
                              size_t size) {
    LPointer<AllocateTask> task =
        AsyncAllocate(dom_query, size);
    task.ptr_->Wait();
    std::vector<Block> blocks = task->blocks_.vec();
    CHI_CLIENT->DelTask(task);
    return blocks;
  }
  CHI_TASK_METHODS(Allocate);

  /** Free a section of the block device */
  HSHM_ALWAYS_INLINE
  void AsyncFreeConstruct(FreeTask *task,
                          const TaskNode &task_node,
                          const DomainQuery &dom_query,
                          const Block &block) {
    CHI_CLIENT->ConstructTask<FreeTask>(
        task, task_node, dom_query, id_,
        block);
  }
  HSHM_ALWAYS_INLINE
  void Free(const DomainQuery &dom_query,
            const Block &block) {
    LPointer<FreeTask> task = AsyncFree(dom_query, block);
    task.ptr_->Wait();
    CHI_CLIENT->DelTask(task);
  }
  CHI_TASK_METHODS(Free);

  /** Write to the block device */
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
  void Write(const DomainQuery &dom_query,
             const hipc::Pointer &data,
             size_t off,
             size_t size) {
    LPointer<WriteTask> task = AsyncWrite(dom_query, data, off, size);
    task.ptr_->Wait();
    CHI_CLIENT->DelTask(task);
  }
  CHI_TASK_METHODS(Write);

  /** Read from the block device */
  HSHM_ALWAYS_INLINE
  void AsyncReadConstruct(ReadTask *task,
                          const TaskNode &task_node,
                          const DomainQuery &dom_query,
                          const hipc::Pointer &data,
                          size_t off,
                          size_t size) {
    CHI_CLIENT->ConstructTask<ReadTask>(
        task, task_node, dom_query, id_,
        data, off, size);
  }
  HSHM_ALWAYS_INLINE
  void Read(const DomainQuery &dom_query,
            const hipc::Pointer &data,
            size_t size,
            size_t off) {
    LPointer<ReadTask> task = AsyncRead(dom_query, data, size, off);
    task.ptr_->Wait();
    CHI_CLIENT->DelTask(task);
  }
  CHI_TASK_METHODS(Read);

  /** Periodically poll block device stats */
  HSHM_ALWAYS_INLINE
  void AsyncPollStatsConstruct(PollStatsTask *task,
                               const TaskNode &task_node,
                               const DomainQuery &dom_query,
                               size_t period_ms) {
    CHI_CLIENT->ConstructTask<PollStatsTask>(
        task, task_node, dom_query, id_,
        period_ms);
  }
  HSHM_ALWAYS_INLINE
  BdevStats PollStats(const DomainQuery &dom_query) {
    LPointer<PollStatsTask> task = AsyncPollStats(dom_query, 0);
    task.ptr_->Wait();
    BdevStats stats = task->stats_;
    CHI_CLIENT->DelTask(task);
    return stats;
  }
  CHI_TASK_METHODS(PollStats);
};

}  // namespace chi

#endif  // CHI_bdev_H_
