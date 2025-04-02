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

#include <thallium.hpp>
#include <thallium/serialization/stl/list.hpp>
#include <thallium/serialization/stl/pair.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>

#include "chimaera/api/chimaera_runtime.h"
#include "chimaera/network/serialize.h"
#include "chimaera/work_orchestrator/work_orchestrator.h"
#include "chimaera_admin/chimaera_admin_client.h"
#include "remote_queue/remote_queue_client.h"

namespace chi::remote_queue {

struct RemoteEntry {
  ResolvedDomainQuery res_domain_;
  Task *task_;
};

class Server : public Module {
 public:
  hipc::atomic<int> pending_ = 0;
  std::vector<hshm::mpsc_queue<RemoteEntry>> submit_;
  std::vector<hshm::mpsc_queue<RemoteEntry>> complete_;
  std::vector<FullPtr<ClientSubmitTask>> submitters_;
  std::vector<FullPtr<ServerCompleteTask>> completers_;
  CLS_CONST int kNodeRpcLanes = 0;
  CLS_CONST int kInitRpcLanes = 1;

 public:
  Server() = default;

  /** Construct remote queue */
  void Create(CreateTask *task, RunContext &rctx) {
    // Registering RPCs
    CHI_THALLIUM->RegisterRpc(*CHI_WORK_ORCHESTRATOR->rpc_pool_,
                              "RpcTaskSubmit",
                              [this](const tl::request &req, tl::bulk &bulk,
                                     SegmentedTransfer &xfer) {
                                this->RpcTaskSubmit(req, bulk, xfer);
                              });
    CHI_THALLIUM->RegisterRpc(*CHI_WORK_ORCHESTRATOR->rpc_pool_,
                              "RpcTaskComplete",
                              [this](const tl::request &req, tl::bulk &bulk,
                                     SegmentedTransfer &xfer) {
                                this->RpcTaskComplete(req, bulk, xfer);
                              });
    CHI_REMOTE_QUEUE->Init(id_);

    // Create lanes
    CreateLaneGroup(kNodeRpcLanes, 1, QUEUE_HIGH_LATENCY);
    CreateLaneGroup(kInitRpcLanes, 4, QUEUE_LOW_LATENCY);

    // Creating submitter and completer queues
    DomainQuery dom_query =
        DomainQuery::GetDirectId(SubDomainId::kContainerSet, container_id_);
    QueueManagerInfo &qm = CHI_QM->config_->queue_manager_;
    submit_.emplace_back(qm.queue_depth_);
    complete_.emplace_back(qm.queue_depth_);
    submitters_.emplace_back(
        CHI_REMOTE_QUEUE->AsyncClientSubmit(HSHM_MCTX, dom_query));
    completers_.emplace_back(
        CHI_REMOTE_QUEUE->AsyncServerComplete(HSHM_MCTX, dom_query));
  }
  void MonitorCreate(MonitorModeId mode, CreateTask *task, RunContext &rctx) {}

  /** Route a task to a lane */
  Lane *MapTaskToLane(const Task *task) override {
    if (task->IsLongRunning()) {
      return GetLaneByHash(kNodeRpcLanes, task->prio_,
                           hshm::hash<DomainQuery>()(task->dom_query_));
    } else {
      return GetLaneByHash(kInitRpcLanes, task->prio_,
                           hshm::hash<DomainQuery>()(task->dom_query_));
    }
  }

  /** Destroy remote queue */
  void Destroy(DestroyTask *task, RunContext &rctx) {}
  void MonitorDestroy(MonitorModeId mode, DestroyTask *task, RunContext &rctx) {
  }

  /** Send the task directly to a node */
  void Direct(Task *submit_task, Task *orig_task,
              ResolvedDomainQuery &res_query, RunContext &rctx) {
    // Save the original domain query
    DomainQuery orig_dom_query = orig_task->dom_query_;
    orig_task->dom_query_ = res_query.dom_;

    // Register the block
    submit_task->SetBlocked(1);
    orig_task->rctx_.remote_pending_ = submit_task;

    // Submit to the new domain
    size_t node_hash = hshm::hash<NodeId>{}(res_query.node_);
    auto &submit = submit_;
    submit[node_hash % submit.size()].emplace(
        (RemoteEntry){res_query, orig_task});

    // Actually block
    submit_task->Yield();

    // Restore the original domain query
    orig_task->dom_query_ = orig_dom_query;
  }

  /** Replicate the task across a node set */
  void Replicate(Task *submit_task, Task *orig_task,
                 std::vector<ResolvedDomainQuery> &dom_queries,
                 RunContext &rctx) {
    std::vector<FullPtr<Task>> replicas;
    replicas.reserve(dom_queries.size());

    // Get the container
    Container *exec = CHI_MOD_REGISTRY->GetStaticContainer(orig_task->pool_);

    // Register the block
    submit_task->SetBlocked(dom_queries.size());
    // CHI_WORK_ORCHESTRATOR->Block(submit_task, rctx);

    // Replicate and submit task
    for (ResolvedDomainQuery &res_query : dom_queries) {
      FullPtr<Task> rep_task;
      exec->NewCopyStart(orig_task->method_, orig_task, rep_task, false);
      if (res_query.dom_.flags_.Any(DomainQuery::kLocal)) {
        exec->Monitor(MonitorMode::kReplicaStart, orig_task->method_, orig_task,
                      rctx);
      }
      rep_task->rctx_.remote_pending_ = submit_task;
      size_t node_hash = hshm::hash<NodeId>{}(res_query.node_);
      auto &submit = submit_;
      HLOG(kInfo, kRemoteQueue, "[TASK_CHECK] Task replica addr {}",
           rep_task.ptr_);
      submit[node_hash % submit.size()].emplace(
          (RemoteEntry){res_query, rep_task.ptr_});
      replicas.emplace_back(rep_task);
    }
    HLOG(kInfo, kRemoteQueue, "[TASK_CHECK] Replicated the submit_task {}",
         submit_task);

    // Actually block
    submit_task->Yield();

    // Combine replicas into the original task
    rctx.replicas_ = &replicas;
    exec->Monitor(MonitorMode::kReplicaAgg, orig_task->method_, orig_task,
                  rctx);
    HLOG(kInfo, kRemoteQueue, "[TASK_CHECK] Back in submit_task {}",
         submit_task);

    // Free replicas
    for (FullPtr<Task> &replica : replicas) {
      CHI_CLIENT->DelTask(HSHM_MCTX, exec, replica.ptr_);
    }
  }

  /** Push operation called on client */
  void ClientPushSubmit(ClientPushSubmitTask *task, RunContext &rctx) {
    HLOG(kInfo, kRemoteQueue, "");
    // Get domain IDs
    Task *orig_task = task->orig_task_;
    std::vector<ResolvedDomainQuery> dom_queries = CHI_RPC->ResolveDomainQuery(
        orig_task->pool_, orig_task->dom_query_, false);

    // Submit task
    if (dom_queries.size() == 0) {
      CHI_CLIENT->ScheduleTask(nullptr, FullPtr<Task>(orig_task));
      return;
    } else if (dom_queries.size() == 1) {
      Direct(task, orig_task, dom_queries[0], rctx);
    } else {
      Replicate(task, orig_task, dom_queries, rctx);
    }
    HLOG(kInfo, kRemoteQueue, "[TASK_CHECK] Pushing back to runtime {}",
         orig_task);

    // Push back to runtime
    if (!orig_task->IsLongRunning()) {
      orig_task->SetTriggerComplete();
    }
    CHI_CLIENT->ScheduleTask(nullptr, FullPtr<Task>(orig_task));
  }
  void MonitorClientPushSubmit(MonitorModeId mode, ClientPushSubmitTask *task,
                               RunContext &rctx) {}

  /** Push operation called on client */
  void ClientSubmit(ClientSubmitTask *task, RunContext &rctx) {
    try {
      RemoteEntry entry;
      std::unordered_map<NodeId, BinaryOutputArchive<true>> entries;
      auto &submit = submit_;
      while (!submit[0].pop(entry).IsNull()) {
        if (entries.find(entry.res_domain_.node_) == entries.end()) {
          entries.emplace(entry.res_domain_.node_, BinaryOutputArchive<true>());
        }
        Task *rep_task = entry.task_;
        // Serialize the task
        Container *exec = CHI_MOD_REGISTRY->GetStaticContainer(rep_task->pool_);
        if (exec == nullptr) {
          HELOG(kFatal, "(node {}) Could not find the pool {}",
                CHI_CLIENT->node_id_, rep_task->pool_);
          return;
        }
        rep_task->dom_query_ = entry.res_domain_.dom_;
        BinaryOutputArchive<true> &ar = entries[entry.res_domain_.node_];
        exec->SaveStart(rep_task->method_, ar, rep_task);
        HLOG(kInfo, kRemoteQueue,
             "[TASK_CHECK] Serializing rep_task {}({} -> {}) ", rep_task,
             CHI_RPC->node_id_, entry.res_domain_.node_);
        // Mark the number of tasks pending to complete
        pending_ += 1;
      }

      for (auto it = entries.begin(); it != entries.end(); ++it) {
        SegmentedTransfer xfer = it->second.Get();
        xfer.ret_node_ = CHI_RPC->node_id_;
        CHI_THALLIUM->SyncIoCall<int>((i32)it->first, "RpcTaskSubmit", xfer,
                                      DT_WRITE);
      }
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Worker {} caught an error: {}",
            CHI_CLIENT->node_id_, id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Worker {} caught an exception: {}",
            CHI_CLIENT->node_id_, id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Worker {} caught an unknown exception",
            CHI_CLIENT->node_id_, id_);
    }
  }
  void MonitorClientSubmit(MonitorModeId mode, ClientSubmitTask *task,
                           RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kFlushWork: {
        rctx.flush_->count_ += submit_.size() + pending_.load();
      }
    }
  }

  /** Complete the task (on the remote node) */
  void ServerPushComplete(ServerPushCompleteTask *task, RunContext &rctx) {
    HLOG(kInfo, kRemoteQueue, "");
    NodeId ret_node = task->rctx_.ret_node_;
    size_t node_hash = hshm::hash<NodeId>{}(ret_node);
    auto &complete = complete_;
    // HILOG(kInfo, "(node {}) Pushing task {}", CHI_CLIENT->node_id_, *task);
    complete[node_hash % complete.size()].emplace(
        (RemoteEntry){ret_node, task});
  }
  void MonitorServerPushComplete(MonitorModeId mode,
                                 ServerPushCompleteTask *task,
                                 RunContext &rctx) {}

  /** Complete the task (on the remote node) */
  void ServerComplete(ServerCompleteTask *task, RunContext &rctx) {
    try {
      // Serialize task completions
      RemoteEntry entry;
      std::unordered_map<NodeId, BinaryOutputArchive<false>> entries;
      auto &complete = complete_;
      std::vector<FullPtr<Task>> done_tasks;
      done_tasks.reserve(complete[0].size());
      while (!complete[0].pop(entry).IsNull()) {
        if (entries.find(entry.res_domain_.node_) == entries.end()) {
          entries.emplace(entry.res_domain_.node_,
                          BinaryOutputArchive<false>());
        }
        Task *done_task = entry.task_;
        Container *exec =
            CHI_MOD_REGISTRY->GetStaticContainer(done_task->pool_);
        BinaryOutputArchive<false> &ar = entries[entry.res_domain_.node_];
        exec->SaveEnd(done_task->method_, ar, done_task);
        done_tasks.emplace_back(done_task);
      }

      // Do transfers
      for (auto it = entries.begin(); it != entries.end(); ++it) {
        SegmentedTransfer xfer = it->second.Get();
        try {
          CHI_THALLIUM->SyncIoCall<int>((i32)it->first, "RpcTaskComplete", xfer,
                                        DT_WRITE);
        } catch (std::exception &e) {
          HELOG(kError, "(node {}) Worker {} caught an exception: {}",
                CHI_CLIENT->node_id_, id_, e.what());
          HELOG(kError, "Current XFER {}", xfer);
        }
      }

      // Free tasks
      for (FullPtr<Task> &task : done_tasks) {
        Container *exec = CHI_MOD_REGISTRY->GetStaticContainer(task->pool_);
        try {
          CHI_CLIENT->DelTask(HSHM_MCTX, exec, task.ptr_);
        } catch (hshm::Error &e) {
          HELOG(kError, "(node {}) Worker {} caught an error: {}",
                CHI_CLIENT->node_id_, id_, e.what());
          HELOG(kError, "(node {}) Was deleting task {} ({})",
                CHI_CLIENT->node_id_, *task.ptr_, task.ptr_);
        }
      }
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Worker {} caught an error: {}",
            CHI_CLIENT->node_id_, id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Worker {} caught an exception: {}",
            CHI_CLIENT->node_id_, id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Worker {} caught an unknown exception",
            CHI_CLIENT->node_id_, id_);
    }
  }
  void MonitorServerComplete(MonitorModeId mode, ServerCompleteTask *task,
                             RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kFlushWork: {
        rctx.flush_->count_ += complete_.size();
      }
    }
  }

 private:
  /** The RPC for processing a message with data */
  void RpcTaskSubmit(const tl::request &req, tl::bulk &bulk,
                     SegmentedTransfer &xfer) {
    try {
      HLOG(kInfo, kRemoteQueue, "");
      xfer.AllocateBulksServer();
      CHI_THALLIUM->IoCallServerWrite(req, bulk, xfer);
      BinaryInputArchive<true> ar(xfer);
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        DeserializeTask(i, ar, xfer);
      }
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Worker {} caught an error: {}",
            CHI_CLIENT->node_id_, id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Worker {} caught an exception: {}",
            CHI_CLIENT->node_id_, id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Worker {} caught an unknown exception",
            CHI_CLIENT->node_id_, id_);
    }
    req.respond(0);
  }

  /** Push operation called at the remote server */
  void DeserializeTask(size_t task_off, BinaryInputArchive<true> &ar,
                       SegmentedTransfer &xfer) {
    // Deserialize task
    PoolId pool_id = xfer.tasks_[task_off].pool_;
    u32 method = xfer.tasks_[task_off].method_;
    Container *exec = CHI_MOD_REGISTRY->GetStaticContainer(pool_id);
    if (exec == nullptr) {
      HELOG(kFatal, "(node {}) Could not find the pool {}",
            CHI_CLIENT->node_id_, pool_id);
      return;
    }
    TaskPointer rep_task = exec->LoadStart(method, ar);
    rep_task->dom_query_ = xfer.tasks_[task_off].dom_;
    rep_task->rctx_.ret_task_addr_ = xfer.tasks_[task_off].task_addr_;
    rep_task->rctx_.ret_node_ = xfer.ret_node_;
    if (rep_task->rctx_.ret_task_addr_ == (size_t)rep_task.ptr_) {
      HELOG(kFatal, "This shouldn't happen ever");
    }
    HLOG(kInfo, kRemoteQueue,
         "[TASK_CHECK] (node {}) Deserialized task {} with replica addr {} "
         "(pool={}, method={})",
         CHI_CLIENT->node_id_, rep_task.ptr_,
         (void *)rep_task->rctx_.ret_task_addr_, pool_id, method);

    // Unset task flags
    // NOTE(llogan): Remote tasks are executed to completion and
    // return values sent back to the remote host. This is
    // for things like long-running monitoring tasks.
    rep_task->SetDataOwner();
    rep_task->UnsetFireAndForget();
    rep_task->UnsetLongRunning();
    rep_task->UnsetRemote();
    rep_task->SetSignalRemoteComplete();
    rep_task->task_flags_.SetBits(TASK_REMOTE_DEBUG_MARK);

    // Execute task
    CHI_CLIENT->ScheduleTask(nullptr, rep_task);
  }

  /** Receive task completion */
  void RpcTaskComplete(const tl::request &req, tl::bulk &bulk,
                       SegmentedTransfer &xfer) {
    try {
      // Deserialize message parameters
      BinaryInputArchive<false> ar(xfer);
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        Task *rep_task = (Task *)xfer.tasks_[i].task_addr_;
        Container *exec = CHI_MOD_REGISTRY->GetStaticContainer(rep_task->pool_);
        if (exec == nullptr) {
          HELOG(kFatal, "(node {}) Could not find the pool {}",
                CHI_CLIENT->node_id_, rep_task->pool_);
          return;
        }
        exec->LoadEnd(rep_task->method_, ar, rep_task);
        HLOG(kInfo, kRemoteQueue, "[TASK_CHECK] Completing replica {}",
             rep_task);
        // Mark the number of tasks pending to complete
        pending_ -= 1;
      }
      // Process bulk message
      try {
        CHI_THALLIUM->IoCallServerWrite(req, bulk, xfer);
      } catch (std::exception &e) {
        HELOG(kError, "(node {}) Worker {} caught an exception: {}",
              CHI_CLIENT->node_id_, id_, e.what());
        HELOG(kError, "Current XFER {}", xfer);
      }
      // Unblock completed tasks
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        Task *rep_task = (Task *)xfer.tasks_[i].task_addr_;
        Task *submit_task = (Task *)rep_task->rctx_.remote_pending_;
        HLOG(kInfo, kRemoteQueue, "[TASK_CHECK] Unblocking the submit_task {}",
             submit_task);
        if (submit_task->pool_ != id_) {
          HELOG(kFatal, "This shouldn't happen ever");
        }
        CHI_WORK_ORCHESTRATOR->SignalUnblock(submit_task, submit_task->rctx_);
      }
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Worker {} caught an error: {}",
            CHI_CLIENT->node_id_, id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Worker {} caught an exception: {}",
            CHI_CLIENT->node_id_, id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Worker {} caught an unknown exception",
            CHI_CLIENT->node_id_, id_);
    }
    req.respond(0);
  }

  CHI_AUTOGEN_METHODS  // keep at class bottom

      public:
#include "remote_queue/remote_queue_lib_exec.h"
};
}  // namespace chi::remote_queue

CHI_TASK_CC(chi::remote_queue::Server, "remote_queue");
