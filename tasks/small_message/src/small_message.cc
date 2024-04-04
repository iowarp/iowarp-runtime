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

namespace chm::small_message {

class Server : public TaskLib {
 public:
  int count_ = 0;
  Client client_;

 public:
  /** Construct small_message */
  void Construct(ConstructTask *task, RunContext &rctx) {
    client_.Init(id_, CHM_ADMIN->queue_id_);
    task->SetModuleComplete();
  }
  void MonitorConstruct(u32 mode, ConstructTask *task, RunContext &rctx) {
  }

  /** Destroy small_message */
  void Destruct(DestructTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorDestruct(u32 mode, DestructTask *task, RunContext &rctx) {
  }

  /** A metadata operation */
  void Md(MdTask *task, RunContext &rctx) {
    if (task->depth_ > 0) {
      LPointer<MdTask> depth_task =
          client_.AsyncMd(task,
                          task->task_node_ + 1,
                          task->domain_id_,
                          task->lane_hash_ + 2,
                          task->depth_ - 1, 0);
      depth_task->Wait<TASK_YIELD_CO>(task);
      HRUN_CLIENT->DelTask(depth_task);
    }
    task->ret_[0] = 1;
    task->SetModuleComplete();
  }
  void MonitorMd(u32 mode, MdTask *task, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kBeginTrainTime: {
//        rctx.timer_.Now();
//        rctx.timer_.Resume();
        break;
      }
      case MonitorMode::kEndTrainTime: {
        // rctx.timer_.Pause();
        // HILOG(kInfo, "Md latency: {} usec", rctx.timer_.GetUsec());
        break;
      }
    }
  }

  /** An I/O task */
  void Io(IoTask *task, RunContext &rctx) {
    task->ret_ = 1;
    char *data = HERMES_MEMORY_MANAGER->Convert<char>(task->data_.shm_);
    (void)data;
    task->SetModuleComplete();
  }
  void MonitorIo(u32 mode, IoTask *task, RunContext &rctx) {
  }

 public:
#include "small_message/small_message_lib_exec.h"
};

}  // namespace chm

HRUN_TASK_CC(chm::small_message::Server, "small_message");
