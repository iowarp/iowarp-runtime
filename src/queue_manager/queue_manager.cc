#include "chimaera/queue_manager/queue_manager.h"

#include "chimaera/api/chimaera_runtime.h"

namespace chi {

#ifdef CHIMAERA_RUNTIME
void QueueManager::ServerInit(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                              NodeId node_id, ServerConfig *config,
                              QueueManagerShm &shm) {
  config_ = config;
  Init(node_id);
  config::QueueManagerInfo &qm = config_->queue_manager_;
  // Initialize ticket queue (ticket 0 is for admin queue)
  max_queues_ = qm.max_queues_;
  max_containers_pn_ = qm.max_containers_pn_;
  // Initialize queue map
  shm.queue_map_.shm_init(alloc);
  queue_map_ = shm.queue_map_.get();
  queue_map_->resize(2);
  // Create the admin queue (used internally by runtime)
  ingress::MultiQueue *queue;
  queue = CreateQueue(
      shm, admin_queue_id_,
      {
          {TaskPrioOpt::kLowLatency, qm.max_containers_pn_,
           qm.max_containers_pn_, qm.queue_depth_, QUEUE_LOW_LATENCY},
          {TaskPrioOpt::kHighLatency, qm.max_containers_pn_,
           qm.max_containers_pn_, qm.queue_depth_, 0},
      });
  queue->flags_.SetBits(QUEUE_READY);
  // Create the process (ingress) queue
  queue = CreateQueue(
      shm, process_queue_id_,
      {
          {TaskPrioOpt::kLowLatency, qm.max_containers_pn_,
           qm.max_containers_pn_, qm.proc_queue_depth_, QUEUE_LOW_LATENCY},
          {TaskPrioOpt::kHighLatency, qm.max_containers_pn_,
           qm.max_containers_pn_, qm.proc_queue_depth_, 0},
      });
  queue->flags_.SetBits(QUEUE_READY);

  int num_gpus = CHI_RUNTIME->ngpu_;
  // Create the ROCm queues
#if defined(CHIMAERA_ENABLE_ROCM) || defined(CHIMAERA_ENABLE_CUDA)
  for (int gpu_id = 0; gpu_id < num_gpus; ++gpu_id) {
    hipc::AllocatorId alloc_id = CHI_RUNTIME->GetGpuCpuAllocId(gpu_id);
    auto *gpu_alloc = HSHM_MEMORY_MANAGER->GetAllocator<CHI_ALLOC_T>(alloc_id);
    QueueManagerShm &gpu_shm =
        gpu_alloc->GetCustomHeader<ChiShm>()->queue_manager_;
    gpu_shm.queue_map_.shm_init(gpu_alloc);
    gpu_shm.queue_map_.get()->resize(2);
    QueueId gpu_queue_id = process_queue_id_;
    gpu_queue_id.group_id_ = gpu_id + 3;
    queue = CreateQueue(
        gpu_shm, gpu_queue_id,
        {
            {TaskPrioOpt::kLowLatency, qm.max_containers_pn_,
             qm.max_containers_pn_, qm.proc_queue_depth_, QUEUE_LOW_LATENCY},
            {TaskPrioOpt::kHighLatency, qm.max_containers_pn_,
             qm.max_containers_pn_, qm.proc_queue_depth_, 0},
        });
    queue->flags_.SetBits(QUEUE_READY);
  }
#endif
}

/** Create a new queue (with pre-allocated ID) in the map */
ingress::MultiQueue *QueueManager::CreateQueue(
    QueueManagerShm &shm, const QueueId &id,
    const std::vector<ingress::PriorityInfo> &queue_info) {
  ingress::MultiQueue *queue = shm.GetQueue(id);
  if (id.IsNull()) {
    HELOG(kError, "Cannot create null queue {}", id);
    return nullptr;
  }
  if (!queue->id_.IsNull()) {
    HELOG(kError, "Queue {} already exists", id);
    return nullptr;
  }
  shm.queue_map_->replace(shm.queue_map_->begin() + id.unique_, id, queue_info);
  return queue;
}
#endif

}  // namespace chi