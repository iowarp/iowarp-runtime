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
#include "queue_factory.h"

namespace chi {

/** Shared-memory representation of the QueueManager */
struct QueueManagerShm {
  hipc::delay_ar<chi::ipc::vector<ingress::MultiQueue>> queue_map_;
  hipc::delay_ar<chi::split_ticket_queue<size_t>> tickets_;
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

 public:
  HSHM_INLINE_CROSS_FUN
  void Init(NodeId node_id) {
    node_id_ = node_id;
    admin_queue_id_ = QueueId(1, 0);
    admin_pool_id_ = PoolId(1, 0);
    process_queue_id_ = QueueId(1, 1);
  }

  /**
   * Get a queue by ID
   *
   * TODO(llogan): Maybe make a local hashtable to map id -> ticket?
   * */
  HSHM_INLINE_CROSS_FUN
  ingress::MultiQueue *GetQueue(const QueueId &id) {
    return &(*queue_map_)[id.unique_];
  }
};

}  // namespace chi

#endif  // CHI_INCLUDE_CHI_QUEUE_MANAGER_QUEUE_MANAGER_H_
