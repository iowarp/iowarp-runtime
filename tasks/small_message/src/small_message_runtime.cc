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

#include "chimaera/api/chimaera_runtime.h"
#include "chimaera/monitor/monitor.h"
#include "chimaera_admin/chimaera_admin_client.h"
#include "small_message/small_message_client.h"

namespace chi::small_message {

class Server : public Module {
 public:
  CLS_CONST LaneGroupId kDefaultGroup = 0;
  int count_ = 0;
  Client client_;
  int upgrade_count_ = 0;
  RollingAverage monitor_[Method::kCount];
  LeastSquares monitor_io_;

 public:
  /** Construct small_message */
  void Create(CreateTask *task, RunContext &rctx) {
    client_.Init(id_, CHI_ADMIN->queue_id_);
    CreateLaneGroup(kDefaultGroup, 4, QUEUE_LOW_LATENCY);

    // Create monitoring functions
    for (int i = 0; i < Method::kCount; ++i) {
      if (i == Method::kIo) continue;
      monitor_[i].Shape(hshm::Formatter::format("{}-method-{}", name_, i));
    }
    monitor_io_.Shape(
        hshm::Formatter::format("{}-method-{}", name_, Method::kIo), 1, 1, 1,
        "SmallMessage.monitor_io");
  }
  void MonitorCreate(MonitorModeId mode, CreateTask *task, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kEstLoad: {
        rctx.load_.cpu_load_ = monitor_[Method::kCreate].Predict();
        break;
      }
      case MonitorMode::kSampleLoad: {
        monitor_[Method::kCreate].Add(rctx.timer_.GetNsec(), rctx.load_);
        break;
      }
      case MonitorMode::kReinforceLoad: {
        monitor_[Method::kCreate].DoTrain();
        break;
      }
    }
  }

  /** Route a task to a lane */
  Lane *MapTaskToLane(const Task *task) override {
    count_ += 1;
    return GetLaneByHash(kDefaultGroup, task->prio_, count_);
  }

  /** Destroy small_message */
  void Destroy(DestroyTask *task, RunContext &rctx) {}
  void MonitorDestroy(MonitorModeId mode, DestroyTask *task, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kEstLoad: {
        rctx.load_.cpu_load_ = monitor_[Method::kDestroy].Predict();
        break;
      }
      case MonitorMode::kSampleLoad: {
        monitor_[Method::kDestroy].Add(rctx.timer_.GetNsec(), rctx.load_);
        break;
      }
      case MonitorMode::kReinforceLoad: {
        monitor_[Method::kDestroy].DoTrain();
        break;
      }
    }
  }

  /** Upgrade small_message */
  void Upgrade(UpgradeTask *task, RunContext &rctx) {
    auto *old = task->Get<Server>();
    upgrade_count_ = old->upgrade_count_ + 1;
  }
  void MonitorUpgrade(MonitorModeId mode, UpgradeTask *task, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kEstLoad: {
        rctx.load_.cpu_load_ = monitor_[Method::kUpgrade].Predict();
        break;
      }
      case MonitorMode::kSampleLoad: {
        monitor_[Method::kUpgrade].Add(rctx.timer_.GetNsec(), rctx.load_);
        break;
      }
      case MonitorMode::kReinforceLoad: {
        monitor_[Method::kUpgrade].DoTrain();
        break;
      }
    }
  }

  /** A metadata operation */
  void Md(MdTask *task, RunContext &rctx) {
    if (task->depth_ > 0) {
      client_.Md(HSHM_MCTX, task->dom_query_, task->depth_ - 1, 0);
    }
    task->ret_ = 1;
    //    HILOG(kInfo, "Executing small message on worker {}",
    //          CHI_WORK_ORCHESTRATOR->GetCurrentWorker()->id_);
  }
  void MonitorMd(MonitorModeId mode, MdTask *task, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kEstLoad: {
        rctx.load_.cpu_load_ = monitor_[Method::kMd].Predict();
        break;
      }
      case MonitorMode::kSampleLoad: {
        monitor_[Method::kMd].Add(rctx.timer_.GetNsec(), rctx.load_);
        break;
      }
      case MonitorMode::kReinforceLoad: {
        monitor_[Method::kMd].DoTrain();
        break;
      }
      case MonitorMode::kReplicaAgg: {
        std::vector<FullPtr<Task>> &replicas = *rctx.replicas_;
        for (FullPtr<Task> &replica : replicas) {
          auto replica_task = reinterpret_cast<MdTask *>(replica.ptr_);
          task->ret_ = replica_task->ret_;
        }
      }
    }
  }

  /** An I/O task */
  void Io(IoTask *task, RunContext &rctx) {
    FullPtr data(task->data_);
    task->ret_ = 0;
    for (size_t i = 0; i < task->size_; ++i) {
      task->ret_ += data.ptr_[i];
    }
    memset(data.ptr_, 15, task->size_);
  }
  void MonitorIo(MonitorModeId mode, IoTask *task, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kEstLoad: {
        rctx.load_.cpu_load_ = monitor_io_.consts_[0] * task->size_;
        break;
      }
      case MonitorMode::kSampleLoad: {
        monitor_io_.Add({(float)task->size_,
                         // (float)rctx.load_.cpu_load_,
                         (float)rctx.timer_.GetNsec()},
                        rctx.load_);
        break;
      }
      case MonitorMode::kReinforceLoad: {
        if (monitor_io_.DoTrain()) {
          CHI_WORK_ORCHESTRATOR->ImportModule("small_message_monitor");
          CHI_WORK_ORCHESTRATOR->RunMethod("ChimaeraMonitor",
                                           "least_squares_fit", monitor_io_);
        }
        break;
      }
      case MonitorMode::kReplicaAgg: {
        std::vector<FullPtr<Task>> &replicas = *rctx.replicas_;
        for (FullPtr<Task> &replica : replicas) {
          auto replica_task = reinterpret_cast<IoTask *>(replica.ptr_);
          task->ret_ = replica_task->ret_;
        }
      }
    }
  }

  CHI_AUTOGEN_METHODS  // keep at class bottom
      public:
#include "small_message/small_message_lib_exec.h"
};

}  // namespace chi::small_message

CHI_TASK_CC(chi::small_message::Server, "chimaera_small_message");
