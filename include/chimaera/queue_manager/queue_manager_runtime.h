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

#ifndef CHI_INCLUDE_CHI_QUEUE_MANAGER_QUEUE_MANAGER_SERVER_H_
#define CHI_INCLUDE_CHI_QUEUE_MANAGER_QUEUE_MANAGER_SERVER_H_

#include "queue_manager.h"
#include "queue_manager_client.h"

namespace chi {

/** Administrative queuing actions */
class QueueManagerRuntime : public QueueManager {
 public:
  ServerConfig *config_;
  size_t max_queues_;
  size_t max_containers_pn_;
  NodeId node_id_;

 public:
  /** Default constructor */
  QueueManagerRuntime() = default;

  /** Destructor*/
  ~QueueManagerRuntime() = default;

  /** Create queues in shared memory */
  void ServerInit(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, NodeId node_id,
                  ServerConfig *config, QueueManagerShm &shm) {
    config_ = config;
    Init(node_id);
    config::QueueManagerInfo &qm = config_->queue_manager_;
    // Initialize ticket queue (ticket 0 is for admin queue)
    max_queues_ = qm.max_queues_;
    max_containers_pn_ = qm.max_containers_pn_;
    // Initialize queue map
    shm.queue_map_.shm_init(alloc);
    queue_map_ = shm.queue_map_.get();
    queue_map_->resize(max_queues_);
    // Create the admin queue (used internally by runtime)
    ingress::MultiQueue *queue;
    queue = CreateQueue(
        admin_queue_id_,
        {
            {TaskPrioOpt::kLowLatency, qm.max_containers_pn_,
             qm.max_containers_pn_, qm.queue_depth_, QUEUE_LOW_LATENCY},
            {TaskPrioOpt::kHighLatency, qm.max_containers_pn_,
             qm.max_containers_pn_, qm.queue_depth_, 0},
        });
    queue->flags_.SetBits(QUEUE_READY);
    // Create the process (ingress) queue
    queue = CreateQueue(
        process_queue_id_,
        {
            {TaskPrioOpt::kLowLatency, qm.max_containers_pn_,
             qm.max_containers_pn_, qm.proc_queue_depth_, QUEUE_LOW_LATENCY},
            {TaskPrioOpt::kHighLatency, qm.max_containers_pn_,
             qm.max_containers_pn_, qm.proc_queue_depth_, 0},
        });
    queue->flags_.SetBits(QUEUE_READY);
    // Create the CUDA queues
#ifdef CHIMAERA_ENABLE_CUDA
#endif

    // Create the ROCm queues
  }

  /** Create a new queue (with pre-allocated ID) in the map */
  ingress::MultiQueue *CreateQueue(
      const QueueId &id, const std::vector<ingress::PriorityInfo> &queue_info) {
    ingress::MultiQueue *queue = GetQueue(id);
    if (id.IsNull()) {
      HELOG(kError, "Cannot create null queue {}", id);
      return nullptr;
    }
    if (!queue->id_.IsNull()) {
      HELOG(kError, "Queue {} already exists", id);
      return nullptr;
    }
    queue_map_->replace(queue_map_->begin() + id.unique_, id, queue_info);
    return queue;
  }

  /**
   * Remove a queue
   *
   * For now, this function assumes that the queue is not in use.
   * TODO(llogan): don't assume this
   * */
  void DestroyQueue(QueueId &id) {
    queue_map_->erase(queue_map_->begin() + id.unique_);
  }
};

#define CHI_QM_RUNTIME hshm::Singleton<chi::QueueManagerRuntime>::GetInstance()

}  // namespace chi

#endif  // CHI_INCLUDE_CHI_QUEUE_MANAGER_QUEUE_MANAGER_SERVER_H_
