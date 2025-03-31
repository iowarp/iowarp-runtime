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

#ifndef CHI_INCLUDE_CHI_WORK_ORCHESTRATOR_WORK_ORCHESTRATOR_H_
#define CHI_INCLUDE_CHI_WORK_ORCHESTRATOR_WORK_ORCHESTRATOR_H_

#include <thread>

#include "chimaera/chimaera_types.h"
#include "chimaera/network/rpc_thallium.h"
#include "chimaera/queue_manager/queue_manager.h"
#include "reinforce_worker.h"
#include "worker.h"

#ifdef CHIMAERA_ENABLE_PYTHON
#include "chimaera/monitor/python_wrapper.h"
#endif

namespace chi {

typedef ABT_key TlsKey;

struct BlockedTask {
  Task *task_;
  hipc::atomic<int> block_count_;

  BlockedTask() = default;

  BlockedTask(Task *task) : task_(task) {
    block_count_ = task->rctx_.block_count_;
  }
};

class WorkOrchestrator {
 public:
  ServerConfig *config_; /**< The server configuration */
  std::vector<std::unique_ptr<Worker>> workers_;   /**< Workers execute tasks */
  CLS_CONST WorkerId kNullWorkerId = (WorkerId)-1; /**< Null worker id */
  std::unique_ptr<Worker> null_worker_;            /**< Null worker */
  std::vector<Worker *> dworkers_; /**< Core-dedicated workers */
  std::vector<Worker *> oworkers_; /**< Undedicated workers */
  std::unique_ptr<ReinforceWorker>
      reinforce_worker_;             /**< Reinforcement worker */
  std::atomic<bool> kill_requested_; /**< Kill flushing threads eventually */
  std::vector<tl::managed<tl::xstream>> rpc_xstreams_; /**< RPC streams */
  tl::managed<tl::pool> rpc_pool_;                     /**< RPC pool */
  TlsKey worker_tls_key_;              /**< Thread-local storage key */
  std::atomic<bool> flushing_ = false; /**< Flushing in progress */
  size_t monitor_window_ = 0;          /**< Sampling window */
  size_t monitor_gap_ = 0;             /**< Monitoring gap */

 public:
  /** Default constructor */
  WorkOrchestrator() = default;

  /** Destructor */
  ~WorkOrchestrator() = default;

  /** Block a task */
  // void Block(Task *task, RunContext &rctx, size_t count);

  /** Unblock a task */
  void SignalUnblock(Task *task, RunContext &rctx);

  /** Create thread pool */
  void ServerInit(ServerConfig *config);

  /** Finalize thread pool */
  void Join();

  /** Get worker with this id */
  Worker &GetWorker(WorkerId worker_id);

  /** Get overlap workers by hash */
  Worker &GetOverlapWorkerByHash(u32 hash) {
    return *workers_[hash % workers_.size()];
  }

  /** Get dedicated worker by hash */
  Worker &GetDedicatedWorkerByHash(u32 hash) {
    return *dworkers_[hash % dworkers_.size()];
  }

  /** Get worker by hash */
  Worker &GetWorkerByHash(TaskPrio prio, u32 hash) {
    if (prio == TaskPrioOpt::kLowLatency) {
      return GetDedicatedWorkerByHash(hash);
    } else {
      return GetOverlapWorkerByHash(hash);
    }
  }

  /** Get overlap worker round-robin */
  Worker &GetWorkerRoundRobin(TaskPrio prio) {
    static int ohash = 0;
    static int dhash = 0;
    if (prio == TaskPrioOpt::kLowLatency) {
      return GetDedicatedWorkerByHash(dhash++);
    } else {
      return GetOverlapWorkerByHash(dhash++);
    }
  }

  /** Get the number of workers */
  size_t GetNumWorkers();

  /** Get all PIDs of active workers */
  std::vector<int> GetWorkerPids();

  /** Get the complement of worker cores */
  std::vector<int> GetWorkerCoresComplement();

  /** Begin dedicating core s*/
  void DedicateCores();

  /** Begin finalizing the runtime */
  HSHM_INLINE
  void FinalizeRuntime() {
    HILOG(kInfo, "(node {}) Finalizing workers", CHI_RPC->node_id_);
    kill_requested_.store(true);
  }

  /** Whether threads should still be executing */
  HSHM_INLINE
  bool IsAlive() { return !kill_requested_.load(); }

  /** Set the CPU affinity of this worker */
  int SetCpuAffinity(ABT_xstream &xstream, int cpu_id) {
    return ABT_xstream_set_affinity(xstream, 1, &cpu_id);
  }

  /** Make an xstream */
  ABT_xstream MakeXstream() {
    ABT_xstream xstream;
    int ret = ABT_xstream_create(ABT_SCHED_NULL, &xstream);
    if (ret != ABT_SUCCESS) {
      HELOG(kFatal, "Could not create argobots xstream");
    }
    return xstream;
  }

  /** Spawn an argobots thread */
  template <typename FUNC, typename TaskT>
  ABT_thread SpawnAsyncThread(ABT_xstream xstream, FUNC &&func, TaskT *data) {
    ABT_thread tl_thread;
    int ret = ABT_thread_create_on_xstream(xstream, func, (void *)data,
                                           ABT_THREAD_ATTR_NULL, &tl_thread);
    if (ret != ABT_SUCCESS) {
      HELOG(kFatal, "Couldn't spawn worker");
    }
    return tl_thread;
  }

  /** Wait for argobots thread */
  void JoinAsyncThread(ABT_thread tl_thread) { ABT_thread_join(tl_thread); }

  /** Create thread-local storage */
  void CreateThreadLocalBlock() {
    int ret = ABT_key_create(NULL, &worker_tls_key_);
    if (ret != ABT_SUCCESS) {
      HELOG(kFatal, "Could not create thread-local storage");
    }
  }

  /** Set thread-local storage to worker (from id) */
  void SetCurrentWorkerId(WorkerId worker_id) {
    SetCurrentWorker(&GetWorker(worker_id));
  }

  /** Set thread-local storage to worker (from ptr) */
  void SetCurrentWorker(Worker *worker) {
    int ret = ABT_key_set(worker_tls_key_, worker);
    if (ret != ABT_SUCCESS) {
      HELOG(kFatal, "Could not set thread-local storage");
    }
  }

  /** Get currently-executing worker */
  Worker *GetCurrentWorker() {
    Worker *worker;
    int ret = ABT_key_get(worker_tls_key_, (void **)&worker);
    if (ret != ABT_SUCCESS || worker == nullptr) {
      worker = null_worker_.get();
      SetCurrentWorker(worker);
    }
    return worker;
  }

  /** Get currently-executing task */
  Task *GetCurrentTask() {
    Worker *worker = GetCurrentWorker();
    if (worker == null_worker_.get()) {
      return nullptr;
    }
    return worker->cur_task_;
  }

  /** Get the currently-executing lane */
  Lane *GetCurrentLane() {
    Worker *worker = GetCurrentWorker();
    if (worker == null_worker_.get()) {
      return nullptr;
    }
    return worker->cur_lane_;
  }

  /** Get the least-loaded ingress queue */
  ingress::Lane *GetLeastLoadedIngressLane(u32 lane_group_id) {
    ingress::MultiQueue *queue = CHI_QM->GetQueue(chi::ADMIN_QUEUE_ID);
    ingress::LaneGroup &lane_group = queue->groups_[lane_group_id];
    ingress::Lane *min_lane = nullptr;
    float min_load = std::numeric_limits<float>::max();
    for (ingress::Lane &lane : lane_group.lanes_) {
      Worker &worker = GetWorker(lane.worker_id_);
      if (worker.load_ < min_load) {
        min_load = worker.load_;
        min_lane = &lane;
      }
    }
    return min_lane;
  }

  /** Calculate per-worker load */
  std::vector<Load> CalculateLoad();

  /** Get the least-loaded ingress queue */
  ingress::Lane *GetThresholdIngressLane(u32 orig_worker_id,
                                         std::vector<Load> &loads,
                                         u32 lane_group_id) {
    ingress::MultiQueue *queue = CHI_QM->GetQueue(chi::ADMIN_QUEUE_ID);
    ingress::LaneGroup &ig_lane_group = queue->groups_[lane_group_id];
    ingress::Lane *min_lane = nullptr;
    // Find the lane with minimum load
    size_t min_load = std::numeric_limits<size_t>::max();
    for (ingress::Lane &ig_lane : ig_lane_group.lanes_) {
      Worker &worker = GetWorker(ig_lane.worker_id_);
      // Reduce the number of workers
      if (0 < loads[worker.id_].cpu_load_ &&
          loads[worker.id_].cpu_load_ < MICROSECONDS(50)) {
        min_lane = &ig_lane;
        return min_lane;
      }
      // Evenly spread across workers
      if (loads[worker.id_].cpu_load_ < min_load) {
        min_load = loads[worker.id_].cpu_load_;
        min_lane = &ig_lane;
      }
    }
    // Don't migrate if like 10% difference
    if (loads[min_lane->worker_id_].cpu_load_ * 1.1 >=
        loads[orig_worker_id].cpu_load_) {
      return nullptr;
    } else {
      return min_lane;
    }
  }

#ifdef CHIMAERA_ENABLE_PYTHON
  void RegisterPath(const std::string &path);
  void ImportModule(const std::string &name);
  void RunString(const std::string &script);
  void RunFunction(const std::string &func_name, PyDataWrapper &data);
  void RunMethod(const std::string &class_name, const std::string &method_name,
                 PyDataWrapper &data);
#endif

 private:
  void PrepareWorkers();
  void MarkWorkers(std::unordered_map<u32, std::vector<Worker *>> cpu_workers);
  void SpawnReinforceThread();
  void AssignAllQueues();
  void AssignQueueMap(chi::ipc::vector<ingress::MultiQueue> &queue_map);
  void SpawnWorkers();
};

}  // namespace chi

#define CHI_WORK_ORCHESTRATOR \
  hshm::Singleton<chi::WorkOrchestrator>::GetInstance()
#define CHI_CUR_TASK CHI_WORK_ORCHESTRATOR->GetCurrentTask()
#define CHI_CUR_LANE CHI_WORK_ORCHESTRATOR->GetCurrentLane()
#define CHI_CUR_WORKER CHI_WORK_ORCHESTRATOR->GetCurrentWorker()

#endif  // CHI_INCLUDE_CHI_WORK_ORCHESTRATOR_WORK_ORCHESTRATOR_H_
