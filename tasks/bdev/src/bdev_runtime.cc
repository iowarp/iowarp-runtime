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

#include <unistd.h>

#include "bdev/bdev_client.h"
#include "chimaera/api/chimaera_runtime.h"
#include "chimaera/monitor/monitor.h"
#include "chimaera_admin/chimaera_admin_client.h"

#ifdef CHIMAERA_ENABLE_CUDA
#include <cuda_runtime.h>
#include <cufile.h>
#endif

namespace chi::bdev {

struct IoPerf {
  LeastSquares bw_;  // bytes / nsec -> GB / sec
  LeastSquares lat_; // nsec
};

class Server : public Module {
public:
  BlockAllocator alloc_;
  BlockUrl url_;
  int fd_;
#ifdef CHIMAERA_ENABLE_CUDA
  CUfileError_t cf_status_;
  CUfileDescr_t cf_descr_;
  CUfileHandle_t cf_handle_;
#endif
  char *ram_;
  RollingAverage monitor_[Method::kCount];
  CLS_CONST int kRead = 0;
  CLS_CONST int kWrite = 1;
  IoPerf io_perf_[2];
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
    url_.path_ = hshm::Formatter::format("{}.{}", url_.path_, container_id_);
    alloc_.Init(1, dev_size);
    CreateLaneGroup(kMdGroup, 1, QUEUE_LOW_LATENCY);
    CreateLaneGroup(kDataGroup, 32, QUEUE_HIGH_LATENCY);

    // Create monitoring functions
    for (int i = 0; i < Method::kCount; ++i) {
      if (i == Method::kRead || i == Method::kWrite)
        continue;
      monitor_[i].Shape(hshm::Formatter::format("{}-method-{}", name_, i));
    }
    io_perf_[kRead].bw_.Shape(
        hshm::Formatter::format("{}-method-{}-bw", name_, Method::kWrite), 1, 2,
        1, "Bdev.monitor_io");
    io_perf_[kRead].lat_.Shape(
        hshm::Formatter::format("{}-method-{}-lat", name_, Method::kRead), 1, 2,
        1, "Bdev.monitor_io");
    io_perf_[kWrite].bw_.Shape(
        hshm::Formatter::format("{}-method-{}-bw", name_, Method::kWrite), 1, 2,
        1, "Bdev.monitor_io");
    io_perf_[kWrite].lat_.Shape(
        hshm::Formatter::format("{}-method-{}-lat", name_, Method::kRead), 1, 2,
        1, "Bdev.monitor_io");

    // Allocate data
    InitialStats(dev_size);

    HILOG(
        kInfo,
        "\033[32mBdev {} created. Read BW {} GB/sec, Write BW {} GB/sec, Read "
        "Latency {} ns, Write Latency {} ns\033[0m",
        url_.path_, io_perf_[kRead].bw_.consts_[0],
        io_perf_[kWrite].bw_.consts_[0], io_perf_[kRead].lat_.consts_[1],
        io_perf_[kWrite].lat_.consts_[1]);
  }

  /** Open a memory segment */
  void OpenMemory(size_t dev_size) {
    // Allocate memory for ram disk
    ram_ = (char *)malloc(dev_size);
    if (ram_ == nullptr) {
      HELOG(kError, "Failed to allocate memory for bdev");
      return;
    }
  }

  /** Open a POSIX file */
  void OpenPosix(size_t dev_size) {
    // Open file for read & write, no override
    fd_ = open64(url_.path_.c_str(), O_RDWR | O_CREAT, 0666);
    if (ftruncate64(fd_, dev_size) != 0) {
      HELOG(kWarning, "Failed to truncate bdev file: {}", strerror(errno));
    }
  }

  /** Open a CUDA file */
  void OpenCufile(size_t dev_size) {
#ifdef CHIMAERA_ENABLE_CUDA
    cf_status_ = cuFileDriverOpen();
    if (cf_status_.err != CU_FILE_SUCCESS) {
      return;
    }
    memset((void *)&cf_descr_, 0, sizeof(CUfileDescr_t));
    cf_descr_.handle.fd = fd_;
    cf_descr_.type = CU_FILE_HANDLE_TYPE_OPAQUE_FD;
    cf_status_ = cuFileHandleRegister(&cf_handle_, &cf_descr_);
    if (cf_status_.err != CU_FILE_SUCCESS) {
      return;
    }
#endif
  }

  void InitialStats(size_t dev_size) {
    switch (url_.scheme_) {
    case BlockUrl::kFs: {
      ssize_t ret;
      OpenPosix(dev_size);
      OpenCufile(dev_size);

      // Set tuning parameters
      hshm::Timer time;
      lat_cutoff_ = KILOBYTES(16);
      size_t bw_cutoff = MEGABYTES(16);
      std::vector<char> data(bw_cutoff);

      // Write 16KB to the beginning with pwrite
      time.Resume();
      ret = pwrite64(fd_, data.data(), lat_cutoff_, 0);
      fsync(fd_);
      time.Pause();
      io_perf_[kWrite].lat_.consts_[0] = 0;
      io_perf_[kWrite].lat_.consts_[1] = (float)time.GetNsec();
      time.Reset();

      // Write 64MB to the beginning with pwrite
      time.Resume();
      ret = pwrite64(fd_, data.data(), bw_cutoff, 0);
      fsync(fd_);
      time.Pause();
      io_perf_[kWrite].bw_.consts_[0] = (float)(bw_cutoff / time.GetNsec());
      io_perf_[kWrite].bw_.consts_[1] = 0;
      time.Reset();

      // Read 16KB from the beginning with pread
      time.Resume();
      ret = pread64(fd_, data.data(), lat_cutoff_, 0);
      fsync(fd_);
      time.Pause();
      io_perf_[kRead].lat_.consts_[0] = 0;
      io_perf_[kRead].lat_.consts_[1] = (float)time.GetNsec();
      time.Reset();

      // Read 64MB from the beginning with pread
      time.Resume();
      ret = pread64(fd_, data.data(), bw_cutoff, 0);
      fsync(fd_);
      time.Pause();
      io_perf_[kRead].bw_.consts_[0] = (float)(bw_cutoff / time.GetNsec());
      io_perf_[kRead].bw_.consts_[1] = 0;
      time.Reset();
      break;
    }
    case BlockUrl::kRam: {
      OpenMemory(dev_size);

      // Tuning parameters
      hshm::Timer time;
      size_t bw_cutoff = MEGABYTES(16);
      std::vector<char> data(bw_cutoff);

      // Write 1MB to the beginning with pw
      // rite
      time.Resume();
      memcpy(ram_, data.data(), bw_cutoff);
      time.Pause();
      io_perf_[kWrite].bw_.consts_[0] = (float)(bw_cutoff / time.GetNsec());
      io_perf_[kWrite].bw_.consts_[1] = 0;
      time.Reset();

      // Read 1MB from the beginning with pread
      time.Resume();
      memcpy(data.data(), ram_, bw_cutoff);
      time.Pause();
      io_perf_[kRead].bw_.consts_[0] = (float)(bw_cutoff / time.GetNsec());
      io_perf_[kRead].bw_.consts_[1] = 0;
      time.Reset();
      break;
    }
    case BlockUrl::kSpdk: {
      // TODO
      break;
    }
    }
  }
  void MonitorCreate(MonitorModeId mode, CreateTask *task, RunContext &rctx) {
    AverageMonitor(Method::kCreate, mode, rctx);
  }

  /** Route a task to a bdev lane */
  Lane *MapTaskToLane(const Task *task) override {
    switch (task->method_) {
    case Method::kRead:
    case Method::kWrite: {
      return GetLeastLoadedLane(
          kDataGroup, task->prio_,
          [](Load &lhs, Load &rhs) { return lhs.cpu_load_ < rhs.cpu_load_; });
    }
    default: {
      return GetLaneByHash(kMdGroup, task->prio_, 0);
    }
    }
  }

  /** Destroy bdev */
  void Destroy(DestroyTask *task, RunContext &rctx) {}
  void MonitorDestroy(MonitorModeId mode, DestroyTask *task, RunContext &rctx) {
    AverageMonitor(Method::kDestroy, mode, rctx);
  }

  /** Allocate a section of the block device */
  void Allocate(AllocateTask *task, RunContext &rctx) {
    alloc_.Allocate(0, task->size_, task->blocks_, task->total_size_);
  }
  void MonitorAllocate(MonitorModeId mode, AllocateTask *task,
                       RunContext &rctx) {
    AverageMonitor(Method::kAllocate, mode, rctx);
  }

  /** Free a section of the block device */
  void Free(FreeTask *task, RunContext &rctx) { alloc_.Free(0, task->block_); }
  void MonitorFree(MonitorModeId mode, FreeTask *task, RunContext &rctx) {}

  /** Write to a block device with POSIX */
  void WritePosix(WriteTask *task, RunContext &rctx) {
    char *data = HSHM_MEMORY_MANAGER->Convert<char>(task->data_);
    ssize_t ret = pwrite64(fd_, data, task->size_, task->off_);
    if (ret == task->size_) {
      task->success_ = true;
    } else {
      HELOG(kWarning, "Failed to write to bdev (off={}, size={}): {}",
            task->off_, task->size_, strerror(errno));
      task->success_ = false;
    }
  }

  /** Write to a block device with memory */
  void WriteMemory(WriteTask *task, RunContext &rctx) {
    char *data = HSHM_MEMORY_MANAGER->Convert<char>(task->data_);
    memcpy(ram_ + task->off_, data, task->size_);
    task->success_ = true;
  }

  /** Write data with GPU memory */
  void WriteGpu(WriteTask *task, RunContext &rctx) {
#if defined(CHIMAERA_ENABLE_CUDA)
    if (cf_status_.err == CU_FILE_SUCCESS) {
      WriteCufile(task, rctx);
    } else {
      WriteCudaCopy(task, rctx);
    }
#elif defined(CHIMAERA_ENABLE_ROCM)
    WriteRocmCopy(task, rctx);
#else
    HELOG(kWarning, "GPU write not supported");
    task->success_ = false;
#endif
  }

  /** Write by copying from GPU memory (ROCm) */
  void WriteRocmCopy(WriteTask *task, RunContext &rctx) {
#ifdef CHIMAERA_ENABLE_ROCM
    char *gpu_data = HSHM_MEMORY_MANAGER->Convert<char>(task->data_);
    std::vector<char> cpu_data(task->size_);
    hipError_t hip_status = hipMemcpy(cpu_data.data(), gpu_data, task->size_,
                                      hipMemcpyDeviceToHost);
    if (hip_status != hipSuccess) {
      HELOG(kWarning, "hipMemcpy failed. ERROR: {}",
            hipGetErrorString(hip_status));
      task->success_ = false;
      return;
    }
    ssize_t ret = pwrite64(fd_, cpu_data.data(), task->size_, task->off_);
    if (ret == task->size_) {
      task->success_ = true;
    } else {
      HELOG(kWarning, "Failed to write to bdev (off={}, size={}): {}",
            task->off_, task->size_, strerror(errno));
      task->success_ = false;
    }
#endif
  }

  /** Write by copying from GPU memory */
  void WriteCudaCopy(WriteTask *task, RunContext &rctx) {
#ifdef CHIMAERA_ENABLE_CUDA
    char *gpu_data = HSHM_MEMORY_MANAGER->Convert<char>(task->data_);
    std::vector<char> cpu_data(task->size_);
    cudaError_t cuda_status = cudaMemcpy(cpu_data.data(), gpu_data, task->size_,
                                         cudaMemcpyDeviceToHost);
    if (cuda_status != cudaSuccess) {
      HELOG(kWarning, "cudaMemcpy failed. ERROR: {}",
            cudaGetErrorString(cuda_status));
      task->success_ = false;
      return;
    }
    ssize_t ret = pwrite64(fd_, cpu_data.data(), task->size_, task->off_);
    if (ret == task->size_) {
      task->success_ = true;
    } else {
      HELOG(kWarning, "Failed to write to bdev (off={}, size={}): {}",
            task->off_, task->size_, strerror(errno));
      task->success_ = false;
    }
#endif
  }

  /** Write to a CUDA file */
  void WriteCufile(WriteTask *task, RunContext &rctx) {
#ifdef CHIMAERA_ENABLE_CUDA
    CUfileError_t status;
    char *gpu_data = HSHM_MEMORY_MANAGER->Convert<char>(task->data_);
    status = cuFileBufRegister(gpu_data, task->size_, 0);
    if (status.err != CU_FILE_SUCCESS) {
      HELOG(kWarning,
            "cuFileBufRegister failed. ERROR: {}. resorting to bounce-buffer.",
            cufileop_status_error(status.err));
      WriteCudaCopy(task, rctx);
      return;
    }

    ssize_t ret = cuFileWrite(cf_handle_, gpu_data, task->size_, 0, 0);
    if (ret < 0) {
      HELOG(kWarning,
            "cuFileWrite failed. ERROR: {}. resorting to bounce-buffer.",
            cufileop_status_error(status.err));
      WriteCudaCopy(task, rctx);
      return;
    }

    status = cuFileBufDeregister(gpu_data);
    if (status.err != CU_FILE_SUCCESS) {
      HELOG(kWarning, "cuFileBufDeregister failed. ERROR: {}. Ignoring.",
            cufileop_status_error(status.err));
    }
#endif
  }

  /** Write to the block device */
  void Write(WriteTask *task, RunContext &rctx) {
    char *data = HSHM_MEMORY_MANAGER->Convert<char>(task->data_);
    switch (url_.scheme_) {
    case BlockUrl::kFs: {
      if (CHI_CLIENT->IsGpuDataPointer(task->data_)) {
        WriteGpu(task, rctx);
      } else {
        WritePosix(task, rctx);
      }
      break;
    }
    case BlockUrl::kRam: {
      WriteMemory(task, rctx);
    }
    case BlockUrl::kSpdk: {
      break;
    }
    }
  }
  void MonitorWrite(MonitorModeId mode, WriteTask *task, RunContext &rctx) {
    IoMonitor(mode, task->size_, io_perf_[kWrite], rctx);
  }

  /** Read from a block device with POSIX */
  void ReadPosix(ReadTask *task, RunContext &rctx) {
    char *data = HSHM_MEMORY_MANAGER->Convert<char>(task->data_);
    ssize_t ret = pread64(fd_, data, task->size_, task->off_);
    if (ret == task->size_) {
      task->success_ = true;
    } else {
      HELOG(kWarning, "Failed to read from bdev (off={}, size={}): {}",
            task->off_, task->size_, strerror(errno));
      task->success_ = false;
    }
  }

  /** Read from a block device with memory */
  void ReadMemory(ReadTask *task, RunContext &rctx) {
    char *data = HSHM_MEMORY_MANAGER->Convert<char>(task->data_);
    memcpy(data, ram_ + task->off_, task->size_);
    task->success_ = true;
  }

  /** Read from a GPU memory */
  void ReadGpu(ReadTask *task, RunContext &rctx) {
#if defined(CHIMAERA_ENABLE_CUDA)
    if (cf_status_.err == CU_FILE_SUCCESS) {
      ReadCufile(task, rctx);
    } else {
      ReadCudaCopy(task, rctx);
    }
#elif defined(CHIMAERA_ENABLE_ROCM)
    ReadRocmCopy(task, rctx);
#else
    HELOG(kWarning, "GPU read not supported");
    task->success_ = false;
#endif
  }

  /** Read from a CUDA file */
  void ReadCufile(ReadTask *task, RunContext &rctx) {
#ifdef CHIMAERA_ENABLE_CUDA
    CUfileError_t status;
    char *gpu_data = HSHM_MEMORY_MANAGER->Convert<char>(task->data_);
    status = cuFileBufRegister(gpu_data, task->size_, 0);
    if (status.err != CU_FILE_SUCCESS) {
      HELOG(kWarning,
            "cuFileBufRegister failed. ERROR: {}. resorting to bounce-buffer.",
            cufileop_status_error(status.err));
      ReadCudaCopy(task, rctx);
      return;
    }

    ssize_t ret = cuFileRead(cf_handle_, gpu_data, task->size_, 0, 0);
    if (ret < 0) {
      HELOG(kWarning,
            "cuFileRead failed. ERROR: {}. resorting to bounce-buffer.",
            cufileop_status_error(status.err));
      ReadCudaCopy(task, rctx);
      return;
    }

    status = cuFileBufDeregister(gpu_data);
    if (status.err != CU_FILE_SUCCESS) {
      HELOG(kWarning, "cuFileBufDeregister failed. ERROR: {}. Ignoring.",
            cufileop_status_error(status.err));
    }
#endif
  }

  /** Read by copying from GPU memory (ROCm) */
  void ReadRocmCopy(ReadTask *task, RunContext &rctx) {
#ifdef CHIMAERA_ENABLE_ROCM
    char *gpu_data = HSHM_MEMORY_MANAGER->Convert<char>(task->data_);
    std::vector<char> cpu_data(task->size_);
    hipError_t hip_status = hipMemcpy(cpu_data.data(), gpu_data, task->size_,
                                      hipMemcpyDeviceToHost);
    if (hip_status != hipSuccess) {
      HELOG(kWarning, "hipMemcpy failed. ERROR: {}",
            hipGetErrorString(hip_status));
      task->success_ = false;
      return;
    }
    ssize_t ret = pread64(fd_, cpu_data.data(), task->size_, task->off_);
    if (ret == task->size_) {
      task->success_ = true;
    } else {
      HELOG(kWarning, "Failed to read from bdev (off={}, size={}): {}",
            task->off_, task->size_, strerror(errno));
      task->success_ = false;
    }
#endif
  }

  /** Read by copying from GPU memory */
  void ReadCudaCopy(ReadTask *task, RunContext &rctx) {
#ifdef CHIMAERA_ENABLE_CUDA
    char *gpu_data = HSHM_MEMORY_MANAGER->Convert<char>(task->data_);
    std::vector<char> cpu_data(task->size_);
    cudaError_t cuda_status = cudaMemcpy(cpu_data.data(), gpu_data, task->size_,
                                         cudaMemcpyDeviceToHost);
    if (cuda_status != cudaSuccess) {
      HELOG(kWarning, "cudaMemcpy failed. ERROR: {}",
            cudaGetErrorString(cuda_status));
      task->success_ = false;
      return;
    }
    ssize_t ret = pread64(fd_, cpu_data.data(), task->size_, task->off_);
    if (ret == task->size_) {
      task->success_ = true;
    } else {
      HELOG(kWarning, "Failed to read from bdev (off={}, size={}): {}",
            task->off_, task->size_, strerror(errno));
      task->success_ = false;
    }
#endif
  }

  /** Read from the block device */
  void Read(ReadTask *task, RunContext &rctx) {
    char *data = HSHM_MEMORY_MANAGER->Convert<char>(task->data_);
    switch (url_.scheme_) {
    case BlockUrl::kFs: {
      if (CHI_CLIENT->IsGpuDataPointer(task->data_)) {
        ReadGpu(task, rctx);
      } else {
        ReadPosix(task, rctx);
      }
      break;
    }
    case BlockUrl::kRam: {
      break;
    }
    case BlockUrl::kSpdk: {
      break;
    }
    }
  }
  void MonitorRead(MonitorModeId mode, ReadTask *task, RunContext &rctx) {
    IoMonitor(mode, task->size_, io_perf_[kRead], rctx);
  }

  /** Poll block device statistics */
  void PollStats(PollStatsTask *task, RunContext &rctx) {
    task->stats_.read_bw_ = io_perf_[kRead].bw_.consts_[0];
    task->stats_.write_bw_ = io_perf_[kWrite].bw_.consts_[0];
    task->stats_.read_latency_ = io_perf_[kRead].lat_.consts_[1];
    task->stats_.write_latency_ = io_perf_[kWrite].lat_.consts_[1];
    task->stats_.free_ = alloc_.free_size_;
    task->stats_.max_cap_ = alloc_.max_heap_size_;
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
      monitor_[Method::kFree].Add(rctx.timer_->GetNsec(), rctx.load_);
      break;
    }
    case MonitorMode::kReinforceLoad: {
      monitor_[Method::kFree].DoTrain();
      break;
    }
    }
  }

  /** I/O task monitoring */
  void IoMonitor(MonitorModeId mode, size_t io_size, IoPerf &io_perf,
                 RunContext &rctx) {
    switch (mode) {
    case MonitorMode::kEstLoad: {
      if (io_size < lat_cutoff_) {
        rctx.load_.cpu_load_ = io_perf.lat_.consts_[1];
      } else {
        rctx.load_.cpu_load_ = io_perf.bw_.consts_[0] * io_size;
      }
      break;
    }
    case MonitorMode::kSampleLoad: {
      io_perf.bw_.Add({(float)io_size,
                       // (float)rctx.load_.cpu_load_,
                       (float)rctx.timer_->GetNsec()},
                      rctx.load_);
      break;
    }
    case MonitorMode::kReinforceLoad: {
      if (io_perf.bw_.DoTrain()) {
        CHI_WORK_ORCHESTRATOR->ImportModule("bdev_monitor");
        CHI_WORK_ORCHESTRATOR->RunMethod("ChimaeraMonitor", "least_squares_fit",
                                         io_perf.bw_);
      }
      break;
    }
    }
  }

public:
#include "bdev/bdev_lib_exec.h"
};

} // namespace chi::bdev

CHI_TASK_CC(chi::bdev::Server, "bdev");
