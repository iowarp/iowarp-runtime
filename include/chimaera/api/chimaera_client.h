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

#ifndef CHI_INCLUDE_CHI_CLIENT_CHI_CLIENT_H_
#define CHI_INCLUDE_CHI_CLIENT_CHI_CLIENT_H_

#include "chimaera_client_defn.h"

namespace chi {

/** Allocate a buffer */
template<bool FROM_REMOTE>
HSHM_ALWAYS_INLINE
LPointer<char> Client::AllocateBufferSafe(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, size_t size) {
  LPointer<char> p;
  while (true) {
    try {
      p = alloc->AllocateLocalPtr<char>(alloc.ctx_, size);
    } catch (hshm::Error &e) {
      p.shm_.SetNull();
    }
    if (!p.shm_.IsNull()) {
      break;
    }
    if constexpr(FROM_REMOTE) {
      Task::StaticYieldFactory<TASK_YIELD_ABT>();
    }
#ifdef CHIMAERA_RUNTIME
    Task *task = CHI_CUR_TASK;
    task->Yield();
#else
    Task::StaticYieldFactory<TASK_YIELD_STD>();
#endif
  }
  return p;
}

/** Schedule a task locally */
#ifdef CHIMAERA_RUNTIME
template<typename TaskT>
void Client::ScheduleTaskRuntime(Task *parent_task,
                                 LPointer<TaskT> &task,
                                 const QueueId &ig_queue_id) {
  std::vector<ResolvedDomainQuery> resolved =
      CHI_RPC->ResolveDomainQuery(task->pool_, task->dom_query_, false);
  task->YieldInit(parent_task);
  ingress::MultiQueue *queue = GetQueue(ig_queue_id);
  DomainQuery dom_query = resolved[0].dom_;
  if (resolved.size() == 1 && resolved[0].node_ == CHI_RPC->node_id_ &&
      dom_query.flags_.All(DomainQuery::kLocal | DomainQuery::kId)) {
    // Determine the lane the task should map to within container
    ContainerId container_id = dom_query.sel_.id_;
    Container *exec = CHI_MOD_REGISTRY->GetContainer(task->pool_,
                                                      container_id);
    if (!exec) {
      // NOTE(llogan): exec is null if there is an update happening.
      // Just send to some worker for now and do not set as routed.
      ingress::LaneGroup &ig_lane_group = queue->GetGroup(
        task->prio_);
      ingress::Lane &ig_lane = ig_lane_group.GetLane(0);
      ig_lane.emplace(task.shm_);
      return;
    }
    chi::Lane *chi_lane = exec->Route(task.ptr_);
    task->rctx_.route_container_ = container_id;
    task->rctx_.route_lane_ = chi_lane->lane_id_;
    task->SetRouted();

    // Get the worker queue for the lane
    ingress::LaneGroup &ig_lane_group = queue->GetGroup(
        chi_lane->ingress_id_.node_id_);
    ingress::Lane &ig_lane = ig_lane_group.GetLane(
        chi_lane->ingress_id_.unique_);
    ig_lane.emplace(task.shm_);
  } else {
    // Place on whatever queue...
    task->SetRemote();
    task->rctx_.route_lane_.node_id_ = task->prio_;
    ingress::LaneGroup &ig_lane_group = queue->GetGroup(
        task->rctx_.route_lane_.node_id_);
    task->rctx_.route_lane_.unique_ =
        std::hash<chi::DomainQuery>{}(task->dom_query_) %
        ig_lane_group.num_lanes_;
    ingress::Lane &ig_lane = ig_lane_group.GetLane(
        task->rctx_.route_lane_.unique_);
    ig_lane.emplace(task.shm_);
  }
}
#endif

}  // namespace chi

#endif  // CHI_INCLUDE_CHI_CLIENT_CHI_CLIENT_H_
