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
#include "bdev/bdev.h"

namespace chi::bdev {

class Server : public Module {
 public:
  Server() = default;

  /** Construct bdev */
  void Create(CreateTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorCreate(MonitorModeId mode, CreateTask *task, RunContext &rctx) {
  }

  /** Route a task to a bdev lane */
  Lane* Route(const Task *task) override {
    return nullptr;
  }

  /** Destroy bdev */
  void Destroy(DestroyTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorDestroy(MonitorModeId mode, DestroyTask *task, RunContext &rctx) {
  }

  /** Allocate a section of the block device */
  void Allocate(AllocateTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorAllocate(MonitorModeId mode, AllocateTask *task, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kEstLoad: {
        break;
      }
      case MonitorMode::kReinforceLoad: {
        break;
      }
      case MonitorMode::kReplicaAgg: {
        std::vector<LPointer<Task>> &replicas = *rctx.replicas_;
        auto replica = reinterpret_cast<AllocateTask *>(
            replicas[0].ptr_);
      }
    }
  }

  /** Free a section of the block device */
  void Free(FreeTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorFree(MonitorModeId mode, FreeTask *task, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kReplicaAgg: {
        std::vector<LPointer<Task>> &replicas = *rctx.replicas_;
        auto replica = reinterpret_cast<ReadTask *>(
            replicas[0].ptr_);
      }
    }
  }

  /** Write to the block device */
  void Write(WriteTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorWrite(MonitorModeId mode, WriteTask *task, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kReplicaAgg: {
        std::vector<LPointer<Task>> &replicas = *rctx.replicas_;
        auto replica = reinterpret_cast<WriteTask *>(
            replicas[0].ptr_);
      }
    }
  }

  /** Read from the block device */
  void Read(ReadTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorRead(MonitorModeId mode, ReadTask *task, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kReplicaAgg: {
        std::vector<LPointer<Task>> &replicas = *rctx.replicas_;
        auto replica = reinterpret_cast<ReadTask *>(
            replicas[0].ptr_);
      }
    }
  }
 public:
#include "bdev/bdev_lib_exec.h"
};

}  // namespace chi::bdev

CHI_TASK_CC(chi::bdev::Server, "bdev");
