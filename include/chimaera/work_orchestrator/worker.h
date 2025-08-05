#ifndef CHI_WORKER_H_
#define CHI_WORKER_H_

#include <vector>
#include <unordered_map>
#include <thread>
#include "../chimaera_types.h"
#include "../chimaera_task.h"
#include "../chimaera_module.h"

namespace chi {

#ifdef CHIMAERA_RUNTIME

enum class WorkerType {
  kLowLatency,
  kHighLatency,
  kReinforcement,
  kProcessReaper
};

class Worker {
private:
  WorkerType type_;
  int worker_id_;
  int cpu_id_;
  std::thread thread_;
  bool should_stop_;
  std::vector<u32> active_lanes_;
  std::vector<u32> cold_lanes_;

public:
  Worker(WorkerType type, int worker_id, int cpu_id);
  ~Worker() = default;

  void Start();
  void Stop();
  void AddLane(u32 lane_id);
  void MoveLaneToCold(u32 lane_id);
  void MoveLaneToActive(u32 lane_id);

private:
  void WorkerLoop();
  void ExecuteTask(Task* task);
  DomainQuery ResolveDomainQuery(const DomainQuery& query, Task* task);
  bool IsLocalTask(const DomainQuery& query);
  void ForwardToRemoteQueue(Task* task);
  Container* GetContainer(Task* task);
  Lane* RouteTaskToLane(Container* container, Task* task);
  void ExecuteInFiber(Container* container, Task* task, RunContext& rctx);
};

#endif  // CHIMAERA_RUNTIME

}  // namespace chi

#endif  // CHI_WORKER_H_