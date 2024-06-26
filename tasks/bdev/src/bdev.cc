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

class Server : public TaskLib {
 public:
  Server() = default;

  /** Construct bdev */
  void Create(CreateTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorCreate(u32 mode, CreateTask *task, RunContext &rctx) {
  }

  /** Destroy bdev */
  void Destruct(DestructTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorDestruct(u32 mode, DestructTask *task, RunContext &rctx) {
  }

  /** A custom method */
  void Write(WriteTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorWrite(u32 mode, WriteTask *task, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kReplicaAgg: {
        std::vector<LPointer<Task>> &replicas = *rctx.replicas_;
        auto replica = reinterpret_cast<WriteTask *>(
            replicas[0].ptr_);
      }
    }
  }

  /** A custom method */
  void Read(ReadTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorRead(u32 mode, ReadTask *task, RunContext &rctx) {
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
