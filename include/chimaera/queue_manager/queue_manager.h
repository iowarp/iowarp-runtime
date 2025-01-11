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

#ifndef CHI_INCLUDE_CHI_QUEUE_MANAGER_QUEUE_MANAGER_H_
#define CHI_INCLUDE_CHI_QUEUE_MANAGER_QUEUE_MANAGER_H_

#include "chimaera/chimaera_types.h"
#include "chimaera/config/config_server.h"
#include "queue.h"

namespace chi {

/** Singleton queue manager */
#define CHI_QM hshm::Singleton<chi::QueueManager>::GetInstance()

/** Shared-memory representation of the QueueManager */
struct QueueManagerShm {
  hipc::delay_ar<chi::ipc::vector<ingress::MultiQueue>> queue_map_;

  HSHM_INLINE_CROSS_FUN
  ingress::MultiQueue *GetQueue(const QueueId &id) {
    return &(*queue_map_)[id.unique_];
  }
};

/** A base class inherited by Client & Server QueueManagers */
class QueueManager {
 public:
  chi::ipc::vector<ingress::MultiQueue>
      *queue_map_; /**< Queues which directly interact with tasks states */
  NodeId node_id_; /**< The ID of the node this QueueManager is on */
  QueueId
      admin_queue_id_; /**< The queue used to submit administrative requests */
  QueueId process_queue_id_; /**< ID of process queue task */
  PoolId admin_pool_id_;     /**< The ID of the admin queue */
  ServerConfig *config_;
  size_t max_queues_;
  size_t max_containers_pn_;
  hipc::CtxAllocator<CHI_ALLOC_T> alloc_;

 public:
  /**
   * Get a queue by ID
   * */
  HSHM_INLINE_CROSS_FUN
  ingress::MultiQueue *GetQueue(const QueueId &id) {
    return &(*queue_map_)[id.unique_];
  }

  /** Initialize client */
  HSHM_INLINE_CROSS_FUN
  void ClientInit(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                  QueueManagerShm &shm, NodeId node_id) {
    alloc_ = alloc;
    queue_map_ = shm.queue_map_.get();
    Init(node_id);
  }

#ifdef CHIMAERA_RUNTIME
  /** Create queues in shared memory */
  void ServerInit(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, NodeId node_id,
                  ServerConfig *config, QueueManagerShm &shm);

  /** Create a new queue (with pre-allocated ID) in the map */
  ingress::MultiQueue *CreateQueue(
      QueueManagerShm &shm, const QueueId &id,
      const std::vector<ingress::PriorityInfo> &queue_info);

  /**
   * Remove a queue
   *
   * For now, this function assumes that the queue is not in use.
   * TODO(llogan): don't assume this
   * */
  void DestroyQueue(QueueId &id) {
    queue_map_->erase(queue_map_->begin() + id.unique_);
  }
#endif

 private:
  HSHM_INLINE_CROSS_FUN
  void Init(NodeId node_id) {
    node_id_ = node_id;
    admin_queue_id_ = QueueId(1, 0);
    admin_pool_id_ = PoolId(1, 0);
    process_queue_id_ = QueueId(1, 1);
  }
};

}  // namespace chi

#endif  // CHI_INCLUDE_CHI_QUEUE_MANAGER_QUEUE_MANAGER_H_
