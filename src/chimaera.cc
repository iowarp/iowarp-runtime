/**
 * Main Chimaera initialization and global functions
 */

#include "chimaera/chimaera.h"
#include "chimaera/container.h"
#include "chimaera/work_orchestrator.h"

namespace chi {

bool CHIMAERA_CLIENT_INIT() {
  auto* chimaera_manager = CHI_CHIMAERA_MANAGER;
  return chimaera_manager->ClientInit();
}

bool CHIMAERA_RUNTIME_INIT() {
  auto* chimaera_manager = CHI_CHIMAERA_MANAGER;
  return chimaera_manager->ServerInit();
}

// Container method implementations

void Container::ScheduleTaskQueueWithWorkOrchestrator(::chi::TaskQueue* task_queue, QueueId queue_id) {
  // Schedule all lanes in the queue using round-robin scheduler
  auto* work_orchestrator = CHI_WORK_ORCHESTRATOR;
  if (work_orchestrator && work_orchestrator->IsInitialized()) {
    work_orchestrator->RoundRobinTaskQueueScheduler(task_queue);
    HILOG(kDebug, "Container: Scheduled lanes for queue {} with WorkOrchestrator for pool {}", queue_id, pool_id_);
  } else {
    HELOG(kError, "Container: WorkOrchestrator not available for lane scheduling");
  }
}



}  // namespace chi