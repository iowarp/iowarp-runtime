//
// Created by lukemartinlogan on 6/29/23.
//

#ifndef CHI_small_message_H_
#define CHI_small_message_H_

#include "small_message_tasks.h"

namespace chi::small_message {

/** Create admin requests */
class Client : public ModuleClient {
public:
  /** Default constructor */
  HSHM_INLINE_CROSS_FUN
  Client() = default;

  /** Destructor */
  HSHM_INLINE_CROSS_FUN
  ~Client() = default;

  /** Create a small_message */
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

  /** Destroy state + queue */
  HSHM_INLINE_CROSS_FUN
  void Destroy(const hipc::MemContext &mctx, const DomainQuery &dom_query) {
    CHI_ADMIN->DestroyContainer(mctx, dom_query, pool_id_);
  }

  /** Metadata task */
  HSHM_INLINE_CROSS_FUN
  int Md(const hipc::MemContext &mctx, const DomainQuery &dom_query, u32 depth,
         chi::IntFlag flags) {
    FullPtr<MdTask> task = AsyncMd(mctx, dom_query, depth, flags);
    task->Wait();
    int ret = task->ret_;
    CHI_CLIENT->DelTask(mctx, task);
    return ret;
  }
  CHI_TASK_METHODS(Md);

  /** Io task */
  HSHM_INLINE_CROSS_FUN
  void Io(const hipc::MemContext &mctx, const DomainQuery &dom_query,
          size_t io_size, u32 io_flags, size_t &write_ret, size_t &read_ret) {
    FullPtr<IoTask> task = AsyncIo(mctx, dom_query, io_size, io_flags);
    task->Wait();
    write_ret = task->ret_;
    FullPtr data(task->data_);
    read_ret = 0;
    for (size_t i = 0; i < io_size; ++i) {
      read_ret += data.ptr_[i];
    }
    CHI_CLIENT->DelTask(mctx, task);
  }
  CHI_TASK_METHODS(Io)

  CHI_AUTOGEN_METHODS // keep at class bottom
};

} // namespace chi::small_message

#endif // CHI_small_message_H_
