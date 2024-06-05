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

#ifndef HRUN_INCLUDE_CHI_WORK_ORCHESTRATOR_WORK_ORCHESTRATOR_H_
#define HRUN_INCLUDE_CHI_WORK_ORCHESTRATOR_WORK_ORCHESTRATOR_H_

#include "chimaera/chimaera_types.h"
#include "chimaera/queue_manager/queue_manager_runtime.h"
#include "chimaera/network/rpc_thallium.h"
#include <thread>

namespace chi {

class Worker;

class WorkOrchestrator {
 public:
  ServerConfig *config_;  /**< The server configuration */
  std::vector<std::unique_ptr<Worker>> workers_;  /**< Workers execute tasks */
  std::vector<Worker*> dworkers_;   /**< Core-dedicated workers */
  std::vector<Worker*> oworkers_;   /**< Undedicated workers */
  std::atomic<bool> stop_runtime_;  /**< Begin killing the runtime */
  std::atomic<bool> kill_requested_;  /**< Kill flushing threads eventually */
  std::vector<tl::managed<tl::xstream>>
    rpc_xstreams_;  /**< RPC streams */
  tl::managed<tl::pool> rpc_pool_;  /**< RPC pool */


 public:
  /** Default constructor */
  WorkOrchestrator() = default;

  /** Destructor */
  ~WorkOrchestrator() = default;

  /** Create thread pool */
  void ServerInit(ServerConfig *config, QueueManager &qm);

  /** Finalize thread pool */
  void Join();

  /** Get worker with this id */
  Worker& GetWorker(u32 worker_id);

  /** Get the number of workers */
  size_t GetNumWorkers();

  /** Get all PIDs of active workers */
  std::vector<int> GetWorkerPids();

  /** Get the complement of worker cores */
  std::vector<int> GetWorkerCoresComplement();

  /** Begin dedicating core s*/
  void DedicateCores();

  /** Begin finalizing the runtime */
  HSHM_ALWAYS_INLINE
  void FinalizeRuntime() {
    stop_runtime_.store(true);
  }

  /** Whether threads should still be executing */
  HSHM_ALWAYS_INLINE
  bool IsAlive() {
    return !kill_requested_.load();
  }

  /** Whether runtime should still be executing */
  HSHM_ALWAYS_INLINE
  bool IsRuntimeAlive() {
    return !stop_runtime_.load();
  }

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
  template<typename FUNC, typename TaskT>
  ABT_thread SpawnAsyncThread(ABT_xstream xstream, FUNC &&func, TaskT *data) {
    ABT_thread tl_thread;
    int ret = ABT_thread_create_on_xstream(xstream,
                                           func, (void*) data,
                                           ABT_THREAD_ATTR_NULL,
                                           &tl_thread);
    if (ret != ABT_SUCCESS) {
      HELOG(kFatal, "Couldn't spawn worker");
    }
    return tl_thread;
  }

  /** Wait for argobots thread */
  void JoinAsyncThread(ABT_thread tl_thread) {
    ABT_thread_join(tl_thread);
  }
};

}  // namespace chi

#define CHI_WORK_ORCHESTRATOR \
  (&CHI_RUNTIME->work_orchestrator_)

#endif  // HRUN_INCLUDE_CHI_WORK_ORCHESTRATOR_WORK_ORCHESTRATOR_H_
