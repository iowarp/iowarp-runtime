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

#include "chimaera_admin/chimaera_admin.h"
#include "chimaera/api/chimaera_runtime.h"
#include "remote_queue/remote_queue.h"
#include "chimaera/network/serialize.h"

namespace thallium {

/** Serialize I/O type enum */
// SERIALIZE_ENUM(chi::IoType);

}  // namespace thallium

namespace chi::remote_queue {

struct TaskQueueEntry {
  ResolvedDomainQuery res_domain_;
  Task *task_;
};

struct SharedState {
  std::vector<hshm::mpsc_queue<TaskQueueEntry>> submit_;
  std::vector<hshm::mpsc_queue<TaskQueueEntry>> complete_;
  std::vector<LPointer<ClientSubmitTask>> submitters_;
  std::vector<LPointer<ServerCompleteTask>> completers_;

  SharedState(Task *task, size_t queue_depth, size_t num_lanes) {
    submit_.resize(num_lanes,
                   (hshm::mpsc_queue<TaskQueueEntry>) {queue_depth});
    complete_.resize(num_lanes,
                     (hshm::mpsc_queue<TaskQueueEntry>) {queue_depth});
    submitters_.resize(num_lanes);
    completers_.resize(num_lanes);
    for (size_t i = 0; i < num_lanes; ++i) {
      submitters_[i] = CHI_REMOTE_QUEUE->AsyncClientSubmit(
          task, task->task_node_ + 1,
          DomainQuery::GetDirectHash(SubDomainId::kLocalContainers, i));
      completers_[i] = CHI_REMOTE_QUEUE->AsyncServerComplete(
          task, task->task_node_ + 1,
          DomainQuery::GetDirectHash(SubDomainId::kLocalContainers, i));
    }
  }
};

class Server : public TaskLib {
 public:
  std::shared_ptr<SharedState> shared_;

 public:
  Server() = default;

  /** Construct remote queue */
  void Create(CreateTask *task, RunContext &rctx) {
    HILOG(kInfo, "(node {}) Constructing remote queue (task_node={}, task_state={}, method={})",
          CHI_CLIENT->node_id_, task->task_node_, task->pool_, task->method_);
    if (rctx.shared_exec_ == this) {
      HRUN_THALLIUM->RegisterRpc(
          *HRUN_WORK_ORCHESTRATOR->rpc_pool_,
          "RpcTaskSubmit", [this](
              const tl::request &req,
              tl::bulk &bulk,
              SegmentedTransfer &xfer) {
            this->RpcTaskSubmit(req, bulk, xfer);
          });
      HRUN_THALLIUM->RegisterRpc(
          *HRUN_WORK_ORCHESTRATOR->rpc_pool_,
          "RpcTaskComplete", [this](
              const tl::request &req,
              tl::bulk &bulk,
              SegmentedTransfer &xfer) {
            this->RpcTaskComplete(req, bulk, xfer);
          });
      CHI_REMOTE_QUEUE->Init(id_);
      QueueManagerInfo &qm = HRUN_QM_RUNTIME->config_->queue_manager_;
      shared_ = std::make_shared<SharedState>(
          task, qm.queue_depth_, 1);
    } else {
      auto *root = (Server*)rctx.shared_exec_;
      shared_ = root->shared_;
    }
    task->SetModuleComplete();
  }
  void MonitorCreate(u32 mode, CreateTask *task, RunContext &rctx) {
  }

  /** Destroy remote queue */
  void Destruct(DestructTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorDestruct(u32 mode, DestructTask *task, RunContext &rctx) {
  }

  /** Repeat task until success */
  void FirstSuccess(Task *task,
                    Task *orig_task,
                    std::vector<ResolvedDomainQuery> &dom_queries) {
    // Replicate task
    bool deep = false;
    for (ResolvedDomainQuery &dom_query : dom_queries) {
      Container *exec = HRUN_TASK_REGISTRY->GetAnyContainer(
          orig_task->pool_);
      LPointer<Task> replica;
      exec->CopyStart(orig_task->method_, orig_task, replica, deep);
      replica->rctx_.pending_to_ = task;
      size_t lane_hash = std::hash<NodeId>{}(dom_query.node_);
      auto &submit = shared_->submit_;
      submit[lane_hash % submit.size()].emplace(
          (TaskQueueEntry) {dom_query, replica.ptr_});
      // Wait
      task->Wait<TASK_YIELD_CO>(replica, TASK_MODULE_COMPLETE);
      bool success = true;  // TODO(llogan): Check for success
      // Free
      HILOG(kDebug, "Replicas were waited for and completed");
      CHI_CLIENT->DelTask(exec, replica.ptr_);
      if (success) {
        break;
      }
    }
  }

  /** Replicate the task across a node set */
  void Replicate(Task *task,
                 Task *orig_task,
                 std::vector<ResolvedDomainQuery> &dom_queries,
                 RunContext &rctx) {
    std::vector<LPointer<Task>> replicas;
    replicas.reserve(dom_queries.size());
    // Replicate task
    bool deep = dom_queries.size() > 1;
    for (ResolvedDomainQuery &dom_query : dom_queries) {
      LPointer<Task> replica;
      Container *exec = HRUN_TASK_REGISTRY->GetAnyContainer(
          orig_task->pool_);
      exec->CopyStart(orig_task->method_, orig_task, replica, deep);
      if (dom_query.dom_.flags_.Any(DomainQuery::kLocal)) {
        exec->Monitor(MonitorMode::kReplicaStart, orig_task, rctx);
      }
      replica->rctx_.pending_to_ = task;
      size_t lane_hash = std::hash<NodeId>{}(dom_query.node_);
      auto &submit = shared_->submit_;
      submit[lane_hash % submit.size()].emplace(
          (TaskQueueEntry) {dom_query, replica.ptr_});
      replicas.emplace_back(replica);
    }
    // Wait
    task->Wait<TASK_YIELD_CO>(replicas, TASK_MODULE_COMPLETE);
    // Combine
    Container *exec = HRUN_TASK_REGISTRY->GetAnyContainer(
        orig_task->pool_);
    rctx.replicas_ = &replicas;
    exec->Monitor(MonitorMode::kReplicaAgg, orig_task, rctx);
    // Free
    HILOG(kDebug, "Replicas were waited for and completed");
    for (LPointer<Task> &replica : replicas) {
      CHI_CLIENT->DelTask(exec, replica.ptr_);
    }
  }

  /** Push operation called on client */
  void ClientPushSubmit(ClientPushSubmitTask *task, RunContext &rctx) {
    // Get domain IDs
    Task *orig_task = task->orig_task_;
    std::vector<ResolvedDomainQuery> dom_queries =
        CHI_RPC->ResolveDomainQuery(orig_task->pool_,
                                     orig_task->dom_query_,
                                     false);
    if (dom_queries.size() == 0) {
      task->SetModuleComplete();
      Worker::SignalUnblock(orig_task);
      return;
    }

    HILOG(kDebug, "ClientPushTask: {} ({}), Original Task: {} ({})",
          (size_t)task, task, (size_t)orig_task, orig_task);
    // Try task
    // Replicate task
    Replicate(task, orig_task, dom_queries, rctx);

    // Unblock original task
    if (!orig_task->IsLongRunning()) {
      orig_task->SetModuleComplete();
    }
    HILOG(kDebug, "Will unblock the task {} to worker {}",
          (size_t)orig_task, orig_task->rctx_.worker_id_);
    Worker::SignalUnblock(orig_task);

    // Set this task as complete
    task->SetModuleComplete();
  }
  void MonitorClientPushSubmit(u32 mode,
                               ClientPushSubmitTask *task,
                               RunContext &rctx) {
  }

  /** Push operation called on client */
  void ClientSubmit(ClientSubmitTask *task, RunContext &rctx) {
    try {
      TaskQueueEntry entry;
      std::unordered_map<NodeId, BinaryOutputArchive<true>> entries;
      auto &submit = shared_->submit_;
      while (!submit[0].pop(entry).IsNull()) {
        HILOG(kDebug, "(node {}) Submitting task {} ({}) to domain {}",
              CHI_CLIENT->node_id_, entry.task_->task_node_,
              (size_t)entry.task_,
              entry.res_domain_);
        if (entries.find(entry.res_domain_.node_) == entries.end()) {
          entries.emplace(entry.res_domain_.node_, BinaryOutputArchive<true>());
        }
        Task *orig_task = entry.task_;
        Container *exec = HRUN_TASK_REGISTRY->GetAnyContainer(
            orig_task->pool_);
        if (exec == nullptr) {
          HELOG(kFatal, "(node {}) Could not find the task state {}",
                CHI_CLIENT->node_id_, orig_task->pool_);
          return;
        }
        orig_task->dom_query_ = entry.res_domain_.dom_;
        BinaryOutputArchive<true> &ar = entries[entry.res_domain_.node_];
        exec->SaveStart(orig_task->method_, ar, orig_task);
      }

      for (auto it = entries.begin(); it != entries.end(); ++it) {
        SegmentedTransfer xfer = it->second.Get();
        xfer.ret_node_ = CHI_RPC->node_id_;
        hshm::Timer t;
        t.Resume();
        HRUN_THALLIUM->SyncIoCall<int>((i32)it->first,
                                       "RpcTaskSubmit",
                                       xfer,
                                       DT_SENDER_WRITE);
        t.Pause();
        HILOG(kDebug, "(node {}) Submitted tasks in {} usec",
              CHI_CLIENT->node_id_, t.GetUsec());
      }
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Worker {} caught an error: {}", CHI_CLIENT->node_id_, id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Worker {} caught an exception: {}", CHI_CLIENT->node_id_, id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Worker {} caught an unknown exception", CHI_CLIENT->node_id_, id_);
    }
  }
  void MonitorClientSubmit(u32 mode,
                           ClientSubmitTask *task,
                           RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kFlushStat: {
        hshm::mpsc_queue<TaskQueueEntry> &submit = shared_->submit_[0];
        hshm::mpsc_queue<TaskQueueEntry> &complete = shared_->complete_[0];
        rctx.flush_->count_ += submit.GetSize() + complete.GetSize();
      }
    }
  }

  /** Complete the task (on the remote node) */
  void ServerPushComplete(ServerPushCompleteTask *task,
                          RunContext &rctx) {
    HILOG(kDebug, "(node {}) Task finished server-side {}",
          CHI_CLIENT->node_id_, task->task_node_);
    if (task->rctx_.ret_task_addr_ == (size_t)task) {
      HILOG(kFatal, "This shouldn't happen ever");
    }
    NodeId ret_node = task->rctx_.ret_node_;
    size_t lane_hash = std::hash<NodeId>{}(ret_node);
    auto &complete = shared_->complete_;
    complete[lane_hash % complete.size()].emplace((TaskQueueEntry){
        ret_node, task
    });
  }
  void MonitorServerPushComplete(u32 mode,
                                 ServerPushCompleteTask *task,
                                 RunContext &rctx) {
  }

  /** Complete the task (on the remote node) */
  void ServerComplete(ServerCompleteTask *task,
                      RunContext &rctx) {
    try {
      // Serialize task completions
      TaskQueueEntry entry;
      std::unordered_map<NodeId, BinaryOutputArchive<false>> entries;
      auto &complete = shared_->complete_;
      size_t count = complete[0].GetSize();
      std::vector<TaskQueueEntry> completed;
      completed.reserve(count);
      while (!complete[0].pop(entry).IsNull()) {
        if (entries.find(entry.res_domain_.node_) == entries.end()) {
          entries.emplace(entry.res_domain_.node_, BinaryOutputArchive<false>());
        }
        Task *done_task = entry.task_;
        HILOG(kDebug, "(node {}) Sending completion for {} -> {}",
              CHI_CLIENT->node_id_, done_task->task_node_,
              entry.res_domain_);
        Container *exec =
            HRUN_TASK_REGISTRY->GetAnyContainer(done_task->pool_);
        BinaryOutputArchive<false> &ar = entries[entry.res_domain_.node_];
        exec->SaveEnd(done_task->method_, ar, done_task);
        completed.emplace_back(entry);
      }

      // Do transfers
      for (auto it = entries.begin(); it != entries.end(); ++it) {
        SegmentedTransfer xfer = it->second.Get();
//        HILOG(kDebug, "(node {}) Sending completion of size {} to {}",
//              CHI_CLIENT->node_id_, xfer.size(),
//              it->first.GetId());
        HRUN_THALLIUM->SyncIoCall<int>((i32)it->first,
                                       "RpcTaskComplete",
                                       xfer,
                                       DT_SENDER_WRITE);
      }

      // Cleanup the queue
//      for (TaskQueueEntry &centry : completed) {
//        Task *done_task = centry.task_;
//        Container *exec =
//            HRUN_TASK_REGISTRY->GetContainer(done_task->pool_);
//        CHI_CLIENT->DelTask(exec, done_task);
//      }
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Worker {} caught an error: {}", CHI_CLIENT->node_id_, id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Worker {} caught an exception: {}", CHI_CLIENT->node_id_, id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Worker {} caught an unknown exception", CHI_CLIENT->node_id_, id_);
    }
  }
  void MonitorServerComplete(u32 mode,
                             ServerCompleteTask *task,
                             RunContext &rctx) {
  }


 private:
  /** The RPC for processing a message with data */
  void RpcTaskSubmit(const tl::request &req,
                     tl::bulk &bulk,
                     SegmentedTransfer &xfer) {
    try {
      xfer.AllocateSegmentsServer();
//      HILOG(kDebug, "(node {}) Received submission of size {}",
//            CHI_CLIENT->node_id_, xfer.size());
      HRUN_THALLIUM->IoCallServerWrite(req, bulk, xfer);
      BinaryInputArchive<true> ar(xfer);
      hshm::Timer t;
      t.Resume();
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        DeserializeTask(i, ar, xfer);
      }
      t.Pause();
      HILOG(kInfo, "(node {}) Submitted tasks in {} usec",
            CHI_CLIENT->node_id_, t.GetUsec());
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Worker {} caught an error: {}", CHI_CLIENT->node_id_, id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Worker {} caught an exception: {}", CHI_CLIENT->node_id_, id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Worker {} caught an unknown exception", CHI_CLIENT->node_id_, id_);
    }
    req.respond(0);
  }

  /** Push operation called at the remote server */
  void DeserializeTask(size_t task_off,
                       BinaryInputArchive<true> &ar,
                       SegmentedTransfer &xfer) {
    // Deserialize task
    PoolId pool_id = xfer.tasks_[task_off].pool_;
    u32 method = xfer.tasks_[task_off].method_;
    Container *exec = HRUN_TASK_REGISTRY->GetAnyContainer(pool_id);
    if (exec == nullptr) {
      HELOG(kFatal, "(node {}) Could not find the task state {}",
            CHI_CLIENT->node_id_, pool_id);
      return;
    }
    TaskPointer task_ptr = exec->LoadStart(method, ar);
    Task *orig_task = task_ptr.ptr_;
    orig_task = task_ptr.ptr_;
    orig_task->dom_query_ = xfer.tasks_[task_off].dom_;
    orig_task->rctx_.ret_task_addr_ = xfer.tasks_[task_off].task_addr_;
    orig_task->rctx_.ret_node_ = xfer.ret_node_;
    if (orig_task->rctx_.ret_task_addr_ == (size_t)orig_task) {
      HILOG(kFatal, "This shouldn't happen ever");
    }

    // Unset task flags
    // NOTE(llogan): Remote tasks are executed to completion and
    // return values sent back to the remote host. This is
    // for things like long-running monitoring tasks.
    orig_task->UnsetStarted();
    orig_task->UnsetSignalUnblock();
    orig_task->UnsetBlocked();
    orig_task->UnsetRemote();
    orig_task->SetDataOwner();
    orig_task->UnsetFireAndForget();
    orig_task->UnsetLongRunning();
    orig_task->SetSignalRemoteComplete();
    orig_task->task_flags_.SetBits(TASK_REMOTE_DEBUG_MARK);

    // Execute task
    MultiQueue *queue = CHI_CLIENT->GetQueue(QueueId(pool_id));
    HILOG(kDebug,
          "(node {}) Enqueuing task (addr={}, task_node={}, task_state={}/{}, "
          "pool_name={}, method={}, size={}, domain={})",
          CHI_CLIENT->node_id_,
          (size_t)orig_task,
          orig_task->task_node_,
          orig_task->pool_,
          pool_id,
          exec->name_,
          method,
          xfer.size(),
          orig_task->dom_query_);
    queue->Emplace(orig_task->prio_,
                   orig_task->GetContainerId(), task_ptr.shm_);
//    HILOG(kDebug,
//          "(node {}) Done submitting (task_node={}, task_state={}/{}, "
//          "pool_name={}, method={}, size={}, lane_hash={})",
//          CHI_CLIENT->node_id_,
//          orig_task->task_node_,
//          orig_task->pool_,
//          pool_id,
//          exec->name_,
//          method,
//          xfer.size(),
//          orig_task->GetContainerId());
  }

  /** Receive task completion */
  void RpcTaskComplete(const tl::request &req,
                       tl::bulk &bulk,
                       SegmentedTransfer &xfer) {
    try {
      HILOG(kDebug, "(node {}) Received completion of size {}",
            CHI_CLIENT->node_id_, xfer.size());
      // Deserialize message parameters
      BinaryInputArchive<false> ar(xfer);
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        Task *orig_task = (Task*)xfer.tasks_[i].task_addr_;
        HILOG(kDebug, "(node {}) Deserializing return values for task {} (state {})",
              CHI_CLIENT->node_id_, orig_task->task_node_, orig_task->pool_);
        Container *exec = HRUN_TASK_REGISTRY->GetAnyContainer(
            orig_task->pool_);
        if (exec == nullptr) {
          HELOG(kFatal, "(node {}) Could not find the task state {}",
                CHI_CLIENT->node_id_, orig_task->pool_);
          return;
        }
        exec->LoadEnd(orig_task->method_, ar, orig_task);
      }
      // Process bulk message
      HRUN_THALLIUM->IoCallServerWrite(req, bulk, xfer);
      // Unblock completed tasks
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        Task *orig_task = (Task*)xfer.tasks_[i].task_addr_;
        orig_task->SetModuleComplete();
        Task *pending_to = orig_task->rctx_.pending_to_;
        if (pending_to->pool_ != id_) {
          HELOG(kFatal, "This shouldn't happen ever");
        }
        HILOG(kDebug, "(node {}) Unblocking task {} to worker {}",
              CHI_CLIENT->node_id_,
              (size_t)pending_to,
              pending_to->rctx_.worker_id_);
        Worker::SignalUnblock(pending_to);
      }
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Worker {} caught an error: {}", CHI_CLIENT->node_id_, id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Worker {} caught an exception: {}", CHI_CLIENT->node_id_, id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Worker {} caught an unknown exception", CHI_CLIENT->node_id_, id_);
    }
    req.respond(0);
  }

 public:
#include "remote_queue/remote_queue_lib_exec.h"
};
}  // namespace chi

HRUN_TASK_CC(chi::remote_queue::Server, "remote_queue");
