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
  std::atomic<size_t> sreqs_ = 0, creqs_ = 0;

  SharedState(Task *task, size_t queue_depth, size_t num_lanes) {
    submit_.resize(num_lanes,
                   (hshm::mpsc_queue<TaskQueueEntry>) {queue_depth});
    complete_.resize(num_lanes,
                     (hshm::mpsc_queue<TaskQueueEntry>) {queue_depth});
    submitters_.reserve(num_lanes);
    completers_.reserve(num_lanes);
  }

  void AddAggregators(Task *task, TaskLib *exec) {
    if (submitters_.size() == complete_.size()) {
      return;
    }
    DomainQuery dom_query = DomainQuery::GetDirectHash(
        SubDomainId::kContainerSet,
        exec->container_id_);
    submitters_.emplace_back(
        CHI_REMOTE_QUEUE->AsyncClientSubmit(
            task, task->task_node_ + 1, dom_query));
    completers_.emplace_back(
        CHI_REMOTE_QUEUE->AsyncServerComplete(
            task, task->task_node_ + 1, dom_query));
  }
};

class Server : public TaskLib {
 public:
  std::shared_ptr<SharedState> shared_;

 public:
  Server() = default;

  /** Construct state shared across containers on this node */
  void CreateNodeState() {

  }

  /** Construct remote queue */
  void Create(CreateTask *task, RunContext &rctx) {
    if (rctx.shared_exec_ == this) {
      CHI_THALLIUM->RegisterRpc(
          *CHI_WORK_ORCHESTRATOR->rpc_pool_,
          "RpcTaskSubmit", [this](
              const tl::request &req,
              tl::bulk &bulk,
              SegmentedTransfer &xfer) {
            this->RpcTaskSubmit(req, bulk, xfer);
          });
      CHI_THALLIUM->RegisterRpc(
          *CHI_WORK_ORCHESTRATOR->rpc_pool_,
          "RpcTaskComplete", [this](
              const tl::request &req,
              tl::bulk &bulk,
              SegmentedTransfer &xfer) {
            this->RpcTaskComplete(req, bulk, xfer);
          });
      CHI_REMOTE_QUEUE->Init(id_);
      QueueManagerInfo &qm = CHI_QM_RUNTIME->config_->queue_manager_;
      shared_ = std::make_shared<SharedState>(
          task, qm.queue_depth_, qm.max_containers_pn_);
    } else {
      auto *root = (Server*)rctx.shared_exec_;
      shared_ = root->shared_;
      shared_->AddAggregators(task, this);
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

  /** Replicate the task across a node set */
  void Replicate(Task *submit_task,
                 Task *orig_task,
                 std::vector<ResolvedDomainQuery> &dom_queries,
                 RunContext &rctx) {
    std::vector<LPointer<Task>> replicas;
    replicas.reserve(dom_queries.size());
    // Replicate task
    bool deep = dom_queries.size() > 1;
    for (ResolvedDomainQuery &res_query : dom_queries) {
      LPointer<Task> rep_task;
      Container *exec = CHI_TASK_REGISTRY->GetAnyContainer(
          orig_task->pool_);
      exec->CopyStart(orig_task->method_, orig_task, rep_task, deep);
      if (res_query.dom_.flags_.Any(DomainQuery::kLocal)) {
        exec->Monitor(MonitorMode::kReplicaStart, orig_task, rctx);
      }
      rep_task->rctx_.pending_to_ = submit_task;
      size_t node_hash = std::hash<NodeId>{}(res_query.node_);
      auto &submit = shared_->submit_;
      submit[node_hash % submit.size()].emplace(
          (TaskQueueEntry) {res_query, rep_task.ptr_});
      replicas.emplace_back(rep_task);
      ++shared_->sreqs_;
      HILOG(kInfo, "[TASK_CHECK] Replicated task {} ({} -> {})",
            rep_task.ptr_, CHI_RPC->node_id_, res_query.node_);
    }
    // Wait
    submit_task->Wait<TASK_YIELD_CO>(replicas, TASK_MODULE_COMPLETE);
    // Combine
    Container *exec = CHI_TASK_REGISTRY->GetAnyContainer(
        orig_task->pool_);
    rctx.replicas_ = &replicas;
    exec->Monitor(MonitorMode::kReplicaAgg, orig_task, rctx);
    // Free
    for (LPointer<Task> &replica : replicas) {
      HILOG(kInfo, "[TASK_CHECK] Completing rep_task {} ({} -> {})",
            replica.ptr_, CHI_RPC->node_id_, replica.ptr_->task_node_)
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
    // Handle fire & forget
    bool is_ff = orig_task->IsFireAndForget();
    orig_task->UnsetFireAndForget();

    // Replicate task
    Replicate(task, orig_task, dom_queries, rctx);

    // Handle fire & forget
    if (is_ff) {
      orig_task->SetFireAndForget();
    }

    // Unblock original task
    if (!orig_task->IsLongRunning()) {
      orig_task->SetModuleComplete();
    }
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
        if (entries.find(entry.res_domain_.node_) == entries.end()) {
          entries.emplace(entry.res_domain_.node_, BinaryOutputArchive<true>());
        }
        Task *rep_task = entry.task_;
        Container *exec = CHI_TASK_REGISTRY->GetAnyContainer(
            rep_task->pool_);
        if (exec == nullptr) {
          HELOG(kFatal, "(node {}) Could not find the task state {}",
                CHI_CLIENT->node_id_, rep_task->pool_);
          return;
        }
        rep_task->dom_query_ = entry.res_domain_.dom_;
        BinaryOutputArchive<true> &ar = entries[entry.res_domain_.node_];
        exec->SaveStart(rep_task->method_, ar, rep_task);
        HILOG(kInfo, "[TASK_CHECK] Serializing rep_task {} ({} -> {})",
              rep_task, CHI_RPC->node_id_, entry.res_domain_.node_);
      }

      for (auto it = entries.begin(); it != entries.end(); ++it) {
        SegmentedTransfer xfer = it->second.Get();
        xfer.ret_node_ = CHI_RPC->node_id_;
        CHI_THALLIUM->SyncIoCall<int>((i32)it->first,
                                       "RpcTaskSubmit",
                                       xfer,
                                       DT_SENDER_WRITE);
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
    NodeId ret_node = task->rctx_.ret_node_;
    size_t node_hash = std::hash<NodeId>{}(ret_node);
    auto &complete = shared_->complete_;
    complete[node_hash % complete.size()].emplace((TaskQueueEntry){
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
        Container *exec =
            CHI_TASK_REGISTRY->GetAnyContainer(done_task->pool_);
        BinaryOutputArchive<false> &ar = entries[entry.res_domain_.node_];
        exec->SaveEnd(done_task->method_, ar, done_task);
        completed.emplace_back(entry);
        // CHI_CLIENT->DelTask(done_task)
      }

      // Do transfers
      for (auto it = entries.begin(); it != entries.end(); ++it) {
        SegmentedTransfer xfer = it->second.Get();
        CHI_THALLIUM->SyncIoCall<int>((i32)it->first,
                                       "RpcTaskComplete",
                                       xfer,
                                       DT_SENDER_WRITE);
      }
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
      CHI_THALLIUM->IoCallServerWrite(req, bulk, xfer);
      BinaryInputArchive<true> ar(xfer);
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        DeserializeTask(i, ar, xfer);
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

  /** Push operation called at the remote server */
  void DeserializeTask(size_t task_off,
                       BinaryInputArchive<true> &ar,
                       SegmentedTransfer &xfer) {
    // Deserialize task
    PoolId pool_id = xfer.tasks_[task_off].pool_;
    u32 method = xfer.tasks_[task_off].method_;
    Container *exec = CHI_TASK_REGISTRY->GetAnyContainer(pool_id);
    if (exec == nullptr) {
      HELOG(kFatal, "(node {}) Could not find the task state {}",
            CHI_CLIENT->node_id_, pool_id);
      return;
    }
    TaskPointer rep_task_ptr = exec->LoadStart(method, ar);
    Task *rep_task = rep_task_ptr.ptr_;
    rep_task = rep_task_ptr.ptr_;
    rep_task->dom_query_ = xfer.tasks_[task_off].dom_;
    rep_task->rctx_.ret_task_addr_ = xfer.tasks_[task_off].task_addr_;
    rep_task->rctx_.ret_node_ = xfer.ret_node_;
    if (rep_task->rctx_.ret_task_addr_ == (size_t)rep_task) {
      HELOG(kFatal, "This shouldn't happen ever");
    }
    HILOG(kInfo, "[TASK_CHECK] Deserialized rep_task {} ({} -> {})",
          (void*)rep_task->rctx_.ret_task_addr_,
          rep_task->rctx_.ret_node_, CHI_RPC->node_id_);

    // Unset task flags
    // NOTE(llogan): Remote tasks are executed to completion and
    // return values sent back to the remote host. This is
    // for things like long-running monitoring tasks.
    rep_task->UnsetStarted();
    rep_task->UnsetSignalUnblock();
    rep_task->UnsetBlocked();
    rep_task->UnsetRemote();
    rep_task->SetDataOwner();
    rep_task->UnsetFireAndForget();
    rep_task->UnsetLongRunning();
    rep_task->SetSignalRemoteComplete();
    rep_task->task_flags_.SetBits(TASK_REMOTE_DEBUG_MARK);

    // Execute task
    CHI_CLIENT->ScheduleTaskRuntime(nullptr, rep_task_ptr,
                                    QueueId(pool_id));
//    HILOG(kDebug,
//          "(node {}) Done submitting (task_node={}, task_state={}/{}, "
//          "pool_name={}, method={}, size={}, node_hash={})",
//          CHI_CLIENT->node_id_,
//          rep_task->task_node_,
//          rep_task->pool_,
//          pool_id,
//          exec->name_,
//          method,
//          xfer.size(),
//          rep_task->GetContainerId());
  }

  /** Receive task completion */
  void RpcTaskComplete(const tl::request &req,
                       tl::bulk &bulk,
                       SegmentedTransfer &xfer) {
    try {
      // Deserialize message parameters
      BinaryInputArchive<false> ar(xfer);
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        Task *rep_task = (Task*)xfer.tasks_[i].task_addr_;
        Container *exec = CHI_TASK_REGISTRY->GetAnyContainer(
            rep_task->pool_);
        if (exec == nullptr) {
          HELOG(kFatal, "(node {}) Could not find the task state {}",
                CHI_CLIENT->node_id_, rep_task->pool_);
          return;
        }
        exec->LoadEnd(rep_task->method_, ar, rep_task);
      }
      // Process bulk message
      CHI_THALLIUM->IoCallServerWrite(req, bulk, xfer);
      // Unblock completed tasks
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        Task *rep_task = (Task*)xfer.tasks_[i].task_addr_;
        rep_task->SetModuleComplete();
        Task *submit_task = rep_task->rctx_.pending_to_;
        if (submit_task->pool_ != id_) {
          HELOG(kFatal, "This shouldn't happen ever");
        }
        ++shared_->creqs_;
        HILOG(kInfo, "[TASK_CHECK] Signal complete rep_task {} on node {}",
              rep_task, CHI_RPC->node_id_);
        Worker::SignalUnblock(submit_task);
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

CHI_TASK_CC(chi::remote_queue::Server, "remote_queue");
