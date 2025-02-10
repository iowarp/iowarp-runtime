//
// Created by llogan on 7/29/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_REINFORCE_WORKER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_REINFORCE_WORKER_H_

#include <hermes_shm/util/affinity.h>

#include <thread>

namespace chi {

/** A single thread for reinforcing ML samples */
class ReinforceWorker {
 public:
  std::unique_ptr<std::thread> thread_;

 public:
  explicit ReinforceWorker(int affinity) {
    thread_ = std::make_unique<std::thread>(&ReinforceWorker::Run, this);
    hshm::ProcessAffiner::SetCpuAffinity((int)thread_->native_handle(),
                                         affinity);
  }

  void Run();
  void Reinforce();
};

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_REINFORCE_WORKER_H_
