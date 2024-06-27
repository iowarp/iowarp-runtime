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

#include "chimaera_admin/chimaera_admin.h"
#include "chimaera/api/chimaera_runtime.h"
#include "small_message/small_message.h"

namespace chi::small_message {

class Server : public TaskLib {
 public:
  int count_ = 0;
  Client client_;

 public:
  /** Construct small_message */
  void Create(CreateTask *task, RunContext &rctx) {
    client_.Init(id_, CHI_ADMIN->queue_id_);
    task->SetModuleComplete();
  }
  void MonitorCreate(u32 mode, CreateTask *task, RunContext &rctx) {
  }

  /** Route a task to a bdev lane */
  LaneId Route(const Task *task) override {
    return 0;
  }

  /** Destroy small_message */
  void Destruct(DestructTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorDestruct(u32 mode, DestructTask *task, RunContext &rctx) {
  }

  /** A metadata operation */
  void Md(MdTask *task, RunContext &rctx) {
//    if (task->depth_ > 0) {
//      LPointer<MdTask> depth_task =
//          client_.AsyncMd(task,
//                          task->task_node_ + 1,
//                          task->dom_query_,
//                          task->depth_ - 1, 0);
//      task->Wait<TASK_YIELD_CO>(depth_task);
//      CHI_CLIENT->DelTask(depth_task);
//    }
    task->ret_ = 1;
    task->SetModuleComplete();
  }
  void MonitorMd(u32 mode, MdTask *task, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kReplicaAgg: {
        std::vector<LPointer<Task>> &replicas = *rctx.replicas_;
        for (LPointer<Task> &replica : replicas) {
          auto replica_task = reinterpret_cast<MdTask *>(replica.ptr_);
          task->ret_ = replica_task->ret_;
        }
      }
    }
  }

  /** An I/O task */
  void Io(IoTask *task, RunContext &rctx) {
    char *data = CHI_CLIENT->GetDataPointer(task->data_);
    task->ret_ = 0;
    for (size_t i = 0; i < task->size_; ++i) {
      task->ret_ += data[i];
    }
    memset(data, 15, task->size_);
    task->SetModuleComplete();
  }
  void MonitorIo(u32 mode, IoTask *task, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kReplicaAgg: {
        std::vector<LPointer<Task>> &replicas = *rctx.replicas_;
        for (LPointer<Task> &replica : replicas) {
          auto replica_task = reinterpret_cast<IoTask *>(replica.ptr_);
          task->ret_ = replica_task->ret_;
        }
      }
    }
  }

 public:
#include "small_message/small_message_lib_exec.h"
};

}  // namespace chi

CHI_TASK_CC(chi::small_message::Server, "small_message");
