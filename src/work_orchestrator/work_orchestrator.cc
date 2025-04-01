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

#include "chimaera/work_orchestrator/work_orchestrator.h"

#include "chimaera/api/chimaera_runtime.h"
#include "chimaera/module_registry/task.h"
#include "chimaera/work_orchestrator/worker.h"

namespace chi {

void WorkOrchestrator::ServerInit(ServerConfig *config) {
  config_ = config;
  // blocked_tasks_.Init(config->queue_manager_.queue_depth_);

  // Initialize argobots
  ABT_init(0, nullptr);

  // Create thread-local storage key
  CreateThreadLocalBlock();

  // Monitoring information
  monitor_gap_ = config_->wo_.monitor_gap_;
  monitor_window_ = config_->wo_.monitor_window_;

  PrepareWorkers();
  SpawnReinforceThread();
  AssignAllQueues();
  SpawnWorkers();
  DedicateCores();

  HILOG(kInfo, "(node {}) Started {} workers", CHI_RPC->node_id_,
        workers_.size());
}

/** Creates the execution streams for workers to run on */
void WorkOrchestrator::PrepareWorkers() {
  size_t num_workers = config_->wo_.cpus_.size();
  workers_.reserve(num_workers);
  int worker_id = 0;
  std::unordered_map<u32, std::vector<Worker *>> cpu_workers;
  for (u32 cpu_id : config_->wo_.cpus_) {
    HILOG(kInfo, "Creating worker {}", worker_id);
    ABT_xstream xstream = MakeXstream();
    workers_.emplace_back(std::make_unique<Worker>(worker_id, cpu_id, xstream));
    Worker &worker = *workers_.back();
    worker.EnableContinuousPolling();
    cpu_workers[cpu_id].push_back(&worker);
    ++worker_id;
  }
  null_worker_ = std::make_unique<Worker>(Worker::kNullWorkerId, 0, nullptr);
  MarkWorkers(cpu_workers);
}

/** Marks the workers as low or high latency */
void WorkOrchestrator::MarkWorkers(
    std::unordered_map<u32, std::vector<Worker *>> cpu_workers) {
  for (auto &cpu_work : cpu_workers) {
    std::vector<Worker *> &workers = cpu_work.second;
    if (workers.size() == 1) {
      for (Worker *worker : workers) {
        worker->SetLowLatency();
        dworkers_.emplace_back(worker);
      }
    } else {
      for (Worker *worker : workers) {
        worker->SetHighLatency();
        oworkers_.emplace_back(worker);
      }
    }
  }
}

/** Map GPU-facing and CPU-facing queues to workers */
void WorkOrchestrator::AssignAllQueues() {
  AssignQueueMap(*CHI_QM->queue_map_);
  int ngpu = CHI_RUNTIME->ngpu_;
  for (int gpu_id = 0; gpu_id < ngpu; ++gpu_id) {
    CHI_ALLOC_T *gpu_alloc = CHI_RUNTIME->GetGpuAlloc(gpu_id);
    QueueManagerShm &gpu_shm =
        gpu_alloc->GetCustomHeader<ChiShm>()->queue_manager_;
    AssignQueueMap(*gpu_shm.queue_map_);
  }
}

/** Map a device queue map to workers */
void WorkOrchestrator::AssignQueueMap(
    chi::ipc::vector<ingress::MultiQueue> &queue_map) {
  static size_t count_lowlat = 0;
  static size_t count_highlat = 0;
  for (ingress::MultiQueue &queue : queue_map) {
    if (queue.id_.IsNull() || !queue.flags_.Any(QUEUE_READY)) {
      continue;
    }
    for (ingress::LaneGroup &lane_group : queue.groups_) {
      u32 num_lanes = lane_group.num_lanes_;
      for (LaneId lane_id = lane_group.num_scheduled_; lane_id < num_lanes;
           ++lane_id) {
        WorkerId worker_id;
        if (lane_group.IsLowLatency()) {
          u32 worker_off = count_lowlat % dworkers_.size();
          count_lowlat += 1;
          Worker &worker = *dworkers_[worker_off];
          worker.work_proc_queue_.emplace_back(
              IngressEntry(lane_group.prio_, lane_id, &queue));
          worker_id = worker.id_;
        } else {
          u32 worker_off = count_highlat % oworkers_.size();
          count_highlat += 1;
          Worker &worker = *oworkers_[worker_off];
          worker.work_proc_queue_.emplace_back(
              IngressEntry(lane_group.prio_, lane_id, &queue));
          worker_id = worker.id_;
        }
        ingress::Lane &lane = lane_group.GetLane(lane_id);
        lane.worker_id_ = worker_id;
      }
      lane_group.num_scheduled_ = num_lanes;
    }
  }
}

/** Spawns the workers */
void WorkOrchestrator::SpawnWorkers() {
  // Enable the workers to begin polling
  for (std::unique_ptr<Worker> &worker : workers_) {
    worker->Spawn();
  }

  // Wait for pids to become non-zero
  while (true) {
    bool all_pids_nonzero = true;
    for (std::unique_ptr<Worker> &worker : workers_) {
      if (worker->pid_ == 0) {
        all_pids_nonzero = false;
        break;
      }
    }
    if (all_pids_nonzero) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

/** Spawns the thread for reinforcing models */
void WorkOrchestrator::SpawnReinforceThread() {
  reinforce_worker_ =
      std::make_unique<ReinforceWorker>(config_->wo_.reinforce_cpu_);
  kill_requested_ = false;
  // Create RPC worker threads
  rpc_pool_ = tl::pool::create(tl::pool::access::mpmc);
  for (u32 cpu_id : config_->rpc_.cpus_) {
    tl::managed<tl::xstream> es =
        tl::xstream::create(tl::scheduler::predef::deflt, *rpc_pool_);
    es->set_cpubind(cpu_id);
    rpc_xstreams_.push_back(std::move(es));
  }
  HILOG(kInfo, "(node {}) Worker created RPC pool with {} threads",
        CHI_RPC->node_id_, CHI_RPC->num_threads_);
}

/** Join the workers */
void WorkOrchestrator::Join() {
  kill_requested_.store(true);
  for (std::unique_ptr<Worker> &worker : workers_) {
    worker->Join();
  }
}

/** Get worker with this id */
Worker &WorkOrchestrator::GetWorker(WorkerId worker_id) {
  if (worker_id == Worker::kNullWorkerId) {
    return *null_worker_;
  } else if (worker_id < workers_.size()) {
    return *workers_[worker_id];
  } else {
    HELOG(kError, "Worker {} does not exist", worker_id);
    return *null_worker_;
  }
}

/** Get the number of workers */
size_t WorkOrchestrator::GetNumWorkers() { return workers_.size(); }

/** Get all PIDs of active workers */
std::vector<int> WorkOrchestrator::GetWorkerPids() {
  std::vector<int> pids;
  pids.reserve(workers_.size());
  for (std::unique_ptr<Worker> &worker : workers_) {
    pids.push_back(worker->pid_);
  }
  return pids;
}

/** Get the complement of worker cores */
std::vector<int> WorkOrchestrator::GetWorkerCoresComplement() {
  std::vector<int> cores;
  cores.reserve(HSHM_SYSTEM_INFO->ncpu_);
  for (int i = 0; i < HSHM_SYSTEM_INFO->ncpu_; ++i) {
    cores.push_back(i);
  }
  for (std::unique_ptr<Worker> &worker : workers_) {
    cores.erase(std::remove(cores.begin(), cores.end(), worker->affinity_),
                cores.end());
  }
  return cores;
}

/** Dedicate cores */
void WorkOrchestrator::DedicateCores() {
  hshm::ProcessAffiner affiner;
  std::vector<int> worker_pids = GetWorkerPids();
  std::vector<int> cpu_ids = GetWorkerCoresComplement();
  affiner.IgnorePids(worker_pids);
  affiner.SetCpus(cpu_ids);
  int count = affiner.AffineAll();
  // HILOG(kInfo, "Affining {} processes to {} cores", count, cpu_ids.size());
}

std::vector<Load> WorkOrchestrator::CalculateLoad() {
  // TODO(llogan): Implement
  std::vector<Load> loads(workers_.size());
  return loads;
}

/** Block a task */
// void WorkOrchestrator::Block(Task *task, RunContext &rctx, size_t count) {
//   rctx.block_count_ += count;
// }

/** Unblock a task */
void WorkOrchestrator::SignalUnblock(Task *task, RunContext &rctx) {
  if (!task) {
    HELOG(kFatal, "(node {}) Invalid pending to during signalling unblock",
          CHI_CLIENT->node_id_);
  }
  if (!rctx.route_lane_) {
    HELOG(kFatal, "(node {}) No route lane during unblock",
          CHI_CLIENT->node_id_);
  }
  if (task->IsBlocked()) {
    HELOG(kWarning, "(node {}) Rescheduling a task still marked as blocked",
          CHI_CLIENT->node_id_);
    while (task->IsBlocked()) {
      std::atomic_thread_fence(std::memory_order_acquire);
      continue;
    }
    HILOG(kInfo, "(node {}) Task unblocked", CHI_CLIENT->node_id_);
  }
  ssize_t count = rctx.block_count_.fetch_sub(1) - 1;
  if (count == 0) {
    rctx.route_lane_->push<false>(FullPtr<Task>(task));
  } else if (count < 0) {
    // HELOG(kFatal, "Block count should never be negative");
  }
}

#ifdef CHIMAERA_ENABLE_PYTHON
void WorkOrchestrator::RegisterPath(const std::string &path) {
  CHI_PYTHON->RegisterPath(path);
}
void WorkOrchestrator::ImportModule(const std::string &name) {
  CHI_PYTHON->ImportModule(name);
}
void WorkOrchestrator::RunString(const std::string &script) {
  CHI_PYTHON->RunString(script);
}
void WorkOrchestrator::RunFunction(const std::string &func_name,
                                   PyDataWrapper &data) {
  CHI_PYTHON->RunFunction(func_name, data);
}
void WorkOrchestrator::RunMethod(const std::string &class_name,
                                 const std::string &method_name,
                                 PyDataWrapper &data) {
  CHI_PYTHON->RunMethod(class_name, method_name, data);
}
#endif

}  // namespace chi
