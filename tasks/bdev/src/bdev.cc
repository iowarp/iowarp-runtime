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

#include "bdev/bdev.h"

#include "chimaera/api/chimaera_runtime.h"
#include "chimaera/monitor/monitor.h"
#include "chimaera_admin/chimaera_admin.h"

namespace chi::bdev {

class Server : public Module {
 public:
  BlockAllocator alloc_;
  BlockUrl url_;
  int fd_;
  char *ram_;
  RollingAverage monitor_[Method::kCount];
  LeastSquares monitor_read_bw_;    // bytes / nsec -> GB / sec
  LeastSquares monitor_read_lat_;   // nsec
  LeastSquares monitor_write_bw_;   // bytes / nsec -> GB / sec
  LeastSquares monitor_write_lat_;  // nsec
  size_t lat_cutoff_;
  CLS_CONST LaneGroupId kMdGroup = 0;
  CLS_CONST LaneGroupId kDataGroup = 1;

 public:
  Server() = default;

  /** Construct bdev */
  void Create(CreateTask *task, RunContext &rctx) {
    CreateTaskParams params = task->GetParams();
    std::string url = params.path_.str();
    size_t dev_size = params.size_;
    url_.Parse(url);
    alloc_.Init(1, dev_size);
    CreateLaneGroup(kMdGroup, 1, QUEUE_LOW_LATENCY);
    CreateLaneGroup(kDataGroup, 8, QUEUE_LOW_LATENCY);

    // Create monitoring functions
    for (int i = 0; i < Method::kCount; ++i) {
      if (i == Method::kRead || i == Method::kWrite) continue;
      monitor_[i].Shape(hshm::Formatter::format("{}-method-{}", name_, i));
    }
    monitor_read_bw_.Shape(
        hshm::Formatter::format("{}-method-{}-bw", name_, Method::kWrite), 1, 2,
        1, "Bdev.monitor_io");
    monitor_read_lat_.Shape(
        hshm::Formatter::format("{}-method-{}-lat", name_, Method::kRead), 1, 2,
        1, "Bdev.monitor_io");
    monitor_write_bw_.Shape(
        hshm::Formatter::format("{}-method-{}-bw", name_, Method::kWrite), 1, 2,
        1, "Bdev.monitor_io");
    monitor_write_lat_.Shape(
        hshm::Formatter::format("{}-method-{}-lat", name_, Method::kRead), 1, 2,
        1, "Bdev.monitor_io");

    // Allocate data
    switch (url_.scheme_) {
      case BlockUrl::kFs: {
        ssize_t ret;

        // Open file for read & write, no override
        fd_ = open(url_.path_.c_str(), O_RDWR | O_CREAT, 0666);
        hshm::Timer time;
        lat_cutoff_ = KILOBYTES(16);
        std::vector<char> data(MEGABYTES(1));

        // Write 16KB to the beginning with pwrite
        time.Resume();
        ret = pwrite(fd_, data.data(), KILOBYTES(16), 0);
        fdatasync(fd_);
        time.Pause();
        monitor_write_lat_.consts_[0] = 0;
        monitor_write_lat_.consts_[1] = (float)time.GetNsec();
        time.Reset();

        // Write 1MB to the beginning with pwrite
        time.Resume();
        ret = pwrite(fd_, data.data(), MEGABYTES(1), 0);
        fdatasync(fd_);
        time.Pause();
        monitor_write_bw_.consts_[0] =
            (float)MEGABYTES(1) / (float)time.GetNsec();
        monitor_write_bw_.consts_[1] = 0;
        time.Reset();

        // Read 4KB from the beginning with pread
        time.Resume();
        fdatasync(fd_);
        ret = pread(fd_, data.data(), KILOBYTES(16), 0);
        time.Pause();
        monitor_read_lat_.consts_[0] = 0;
        monitor_read_lat_.consts_[1] = (float)time.GetNsec();
        time.Reset();

        // Read 1MB from the beginning with pread
        time.Resume();
        fdatasync(fd_);
        ret = pread(fd_, data.data(), MEGABYTES(1), 0);
        time.Pause();
        monitor_read_bw_.consts_[0] =
            (float)MEGABYTES(1) / (float)time.GetNsec();
        ;
        monitor_read_bw_.consts_[1] = 0;
        time.Reset();
        break;
      }
      case BlockUrl::kRam: {
        // Malloc memory for ram disk
        ram_ = (char *)malloc(dev_size);
        hshm::Timer time;
        lat_cutoff_ = 0;
        std::vector<char> data(MEGABYTES(1));

        // Write 1MB to the beginning with pwrite
        time.Resume();
        memcpy(ram_, data.data(), MEGABYTES(1));
        time.Pause();
        monitor_write_bw_.consts_[0] =
            (float)time.GetNsec() / (float)MEGABYTES(1);
        monitor_write_bw_.consts_[1] = 0;
        time.Reset();

        // Read 1MB from the beginning with pread
        time.Resume();
        memcpy(data.data(), ram_, MEGABYTES(1));
        time.Pause();
        monitor_read_bw_.consts_[0] =
            (float)time.GetNsec() / (float)MEGABYTES(1);
        monitor_read_bw_.consts_[1] = 0;
        time.Reset();
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
    AverageMonitor(Method::kCreate, mode, rctx);
  }

  /** Route a task to a bdev lane */
  Lane *Route(const Task *task) override {
    switch (task->method_) {
      case Method::kRead:
      case Method::kWrite: {
        return GetLeastLoadedLane(
            kDataGroup, task->prio_,
            [](Load &lhs, Load &rhs) { return lhs.io_load_ < rhs.io_load_; });
      }
      default: {
        return GetLaneByHash(kMdGroup, task->prio_, 0);
      }
    }
  }

  /** Destroy bdev */
  void Destroy(DestroyTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorDestroy(MonitorModeId mode, DestroyTask *task, RunContext &rctx) {
    AverageMonitor(Method::kDestroy, mode, rctx);
  }

  /** Allocate a section of the block device */
  void Allocate(AllocateTask *task, RunContext &rctx) {
    alloc_.Allocate(0, task->size_, task->blocks_, task->total_size_);
    task->SetModuleComplete();
  }
  void MonitorAllocate(MonitorModeId mode, AllocateTask *task,
                       RunContext &rctx) {
    AverageMonitor(Method::kAllocate, mode, rctx);
  }

  /** Free a section of the block device */
  void Free(FreeTask *task, RunContext &rctx) {
    alloc_.Free(0, task->block_);
    task->SetModuleComplete();
  }
  void MonitorFree(MonitorModeId mode, FreeTask *task, RunContext &rctx) {}

  /** Write to the block device */
  void Write(WriteTask *task, RunContext &rctx) {
    char *data = HERMES_MEMORY_MANAGER->Convert<char>(task->data_);
    switch (url_.scheme_) {
      case BlockUrl::kFs: {
        ssize_t ret = pwrite(fd_, data, task->size_, task->off_);
        if (ret == task->size_) {
          task->success_ = true;
        } else {
          task->success_ = false;
        }
        break;
      }
      case BlockUrl::kRam: {
        memcpy(ram_ + task->off_, data, task->size_);
        task->success_ = true;
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
      case MonitorMode::kEstLoad: {
        if (task->size_ < lat_cutoff_) {
        } else {
          rctx.load_.cpu_load_ = monitor_write_bw_.consts_[0] * task->size_;
        }
        break;
      }
      case MonitorMode::kSampleLoad: {
        monitor_write_bw_.Add({(float)task->size_,
                               // (float)rctx.load_.cpu_load_,
                               (float)rctx.timer_.GetNsec()},
                              rctx.load_);
        break;
      }
      case MonitorMode::kReinforceLoad: {
        if (monitor_write_bw_.DoTrain()) {
          CHI_WORK_ORCHESTRATOR->ImportModule("bdev_monitor");
          CHI_WORK_ORCHESTRATOR->RunMethod(
              "ChimaeraMonitor", "least_squares_fit", monitor_write_bw_);
        }
        break;
      }
    }
  }

  /** Read from the block device */
  void Read(ReadTask *task, RunContext &rctx) {
    char *data = HERMES_MEMORY_MANAGER->Convert<char>(task->data_);
    switch (url_.scheme_) {
      case BlockUrl::kFs: {
        ssize_t ret = pread(fd_, data, task->size_, task->off_);
        if (ret == task->size_) {
          task->success_ = true;
        } else {
          task->success_ = false;
        }
        break;
      }
      case BlockUrl::kRam: {
        memcpy(data, ram_ + task->off_, task->size_);
        task->success_ = true;
        break;
      }
      case BlockUrl::kSpdk: {
        break;
      }
    }
    task->SetModuleComplete();
  }
  void MonitorRead(MonitorModeId mode, ReadTask *task, RunContext &rctx) {}

  /** Poll block device statistics */
  void PollStats(PollStatsTask *task, RunContext &rctx) {
    task->stats_.read_bw_ = monitor_read_bw_.consts_[0];
    task->stats_.write_bw_ = monitor_write_bw_.consts_[0];
    task->stats_.read_latency_ = monitor_read_lat_.consts_[1];
    task->stats_.write_latency_ = monitor_write_lat_.consts_[1];
    task->stats_.free_ = alloc_.free_size_;
    task->SetModuleComplete();
  }
  void MonitorPollStats(MonitorModeId mode, PollStatsTask *task,
                        RunContext &rctx) {
    AverageMonitor(Method::kPollStats, mode, rctx);
  }

  /** Rolling average for most tasks */
  void AverageMonitor(MethodId method, MonitorModeId mode, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kEstLoad: {
        rctx.load_.cpu_load_ = monitor_[Method::kFree].Predict();
        break;
      }
      case MonitorMode::kSampleLoad: {
        monitor_[Method::kFree].Add(rctx.timer_.GetNsec(), rctx.load_);
        break;
      }
      case MonitorMode::kReinforceLoad: {
        monitor_[Method::kFree].DoTrain();
        break;
      }
    }
  }

 public:
#include "bdev/bdev_lib_exec.h"
};

}  // namespace chi::bdev

CHI_TASK_CC(chi::bdev::Server, "bdev");
