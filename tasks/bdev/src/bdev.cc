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
  BlockAllocator alloc_;
  BlockUrl url_;
  int fd_;
  char *ram_;

 public:
  Server() = default;

  /** Construct bdev */
  void Create(CreateTask *task, RunContext &rctx) {
    std::string url = task->path_.str();
    size_t dev_size = task->size_;
    url_.Parse(url);
    alloc_.Init(1, dev_size);
    CreateLaneGroup(0, 1, QUEUE_LOW_LATENCY);
    switch (url_.scheme_) {
      case BlockUrl::kFs: {
        // Open file for read & write, no override
        fd_ = open(url_.path_.c_str(), O_RDWR | O_CREAT, 0666);
        hshm::Timer time;
        time.Resume();
        // Write 4KB to the beginning with pwrite
        time.Pause();
        break;
      }
      case BlockUrl::kRam: {
        // Malloc memory for ram disk
        ram_ = (char *)malloc(dev_size);
        break;
      }
      case BlockUrl::kSpdk: {
        // TODO
        break;
      }
    }
    task->SetModuleComplete();
  }
  void MonitorCreate(MonitorModeId mode, CreateTask *task, RunContext &rctx) {
  }

  /** Route a task to a bdev lane */
  Lane* Route(const Task *task) override {
    return GetLaneByHash(task->prio_, 0);
  }

  /** Destroy bdev */
  void Destroy(DestroyTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorDestroy(MonitorModeId mode, DestroyTask *task, RunContext &rctx) {
  }

  /** Allocate a section of the block device */
  void Allocate(AllocateTask *task, RunContext &rctx) {
    task->block_ = alloc_.Allocate(0, task->size_);
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
    alloc_.Free(0, task->block_);
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
    char *data = HERMES_MEMORY_MANAGER->Convert<char>(task->data_);
    switch (url_.scheme_) {
      case BlockUrl::kFs: {
        pwrite(fd_, data, task->size_, task->off_);
        break;
      }
      case BlockUrl::kRam: {
        memcpy(ram_ + task->off_, data, task->size_);
        break;
      }
      case BlockUrl::kSpdk: {
        break;
      }
    }
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
    char *data = HERMES_MEMORY_MANAGER->Convert<char>(task->data_);
    switch (url_.scheme_) {
      case BlockUrl::kFs: {
        pread(fd_, data, task->size_, task->off_);
        break;
      }
      case BlockUrl::kRam: {
        memcpy(data, ram_ + task->off_, task->size_);
        break;
      }
      case BlockUrl::kSpdk: {
        break;
      }
    }
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

  /** Poll block device statistics */
  void PollStats(PollStatsTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorPollStats(MonitorModeId mode,
                        PollStatsTask *task, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kReplicaAgg: {
        std::vector<LPointer<Task>> &replicas = *rctx.replicas_;
        auto replica = reinterpret_cast<PollStatsTask *>(
            replicas[0].ptr_);
      }
    }
  }
 public:
#include "bdev/bdev_lib_exec.h"
};

}  // namespace chi::bdev

CHI_TASK_CC(chi::bdev::Server, "bdev");
