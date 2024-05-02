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

#ifndef HRUN_INCLUDE_HRUN_QUEUE_MANAGER_QUEUE_MANAGER_H_
#define HRUN_INCLUDE_HRUN_QUEUE_MANAGER_QUEUE_MANAGER_H_

#include "chimaera/config/config_server.h"
#include "chimaera/chimaera_types.h"
#include "queue_factory.h"

namespace chm {

/** Shared-memory representation of the QueueManager */
struct QueueManagerShm {
  hipc::ShmArchive<hipc::vector<MultiQueue>> queue_map_;
  hipc::ShmArchive<hipc::split_ticket_queue<size_t>> tickets_;
};

/** A base class inherited by Client & Server QueueManagers */
class QueueManager {
 public:
  hipc::vector<MultiQueue> *queue_map_;   /**< Queues which directly interact with tasks states */
  NodeId node_id_;             /**< The ID of the node this QueueManager is on */
  QueueId admin_queue_id_;     /**< The queue used to submit administrative requests */
  QueueId process_queue_id_;     /**< ID of process queue task */
  TaskStateId admin_task_state_;  /**< The ID of the admin queue */

 public:
  void Init(NodeId node_id) {
    node_id_ = node_id;
    admin_queue_id_ = QueueId(1, 0);
    admin_task_state_ = TaskStateId(1, 0);
    process_queue_id_ = QueueId(1, 1);
  }

  /**
   * Get a queue by ID
   *
   * TODO(llogan): Maybe make a local hashtable to map id -> ticket?
   * */
  HSHM_ALWAYS_INLINE MultiQueue* GetQueue(const QueueId &id) {
    return &(*queue_map_)[id.unique_];
  }
};

}  // namespace chm

#endif  // HRUN_INCLUDE_HRUN_QUEUE_MANAGER_QUEUE_MANAGER_H_
