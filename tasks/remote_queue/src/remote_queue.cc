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
// SERIALIZE_ENUM(chm::IoType);

}  // namespace thallium

namespace chm::remote_queue {

struct RemoteInfo {
  std::atomic<u32> rep_cnt_;
  u32 rep_max_;
  std::vector<LPointer<Task>> replicas_;
  DomainId ret_domain_;
  size_t task_addr_;
};

struct TaskQueueEntry {
  DomainId domain_;
  Task *task_;
};

class Server : public TaskLib {
 public:
  std::vector<hshm::mpsc_queue<TaskQueueEntry>> submit_;
  std::vector<hshm::mpsc_queue<TaskQueueEntry>> complete_;
  std::vector<LPointer<ClientSubmitTask>> submitters_;
  std::vector<LPointer<ServerCompleteTask>> completers_;

 public:
  Server() = default;

  /** Construct remote queue */
  void Construct(ConstructTask *task, RunContext &rctx) {
    HILOG(kInfo, "(node {}) Constructing remote queue (task_node={}, task_state={}, method={})",
          HRUN_CLIENT->node_id_, task->task_node_, task->task_state_, task->method_);
    QueueManagerInfo &qm = HRUN_QM_RUNTIME->config_->queue_manager_;
    // size_t max_lanes = HRUN_QM_RUNTIME->max_lanes_;
    size_t max_lanes = 1;
    submit_.resize(
        max_lanes,
        (hshm::mpsc_queue<TaskQueueEntry>){qm.queue_depth_});
    complete_.resize(
        max_lanes,
        (hshm::mpsc_queue<TaskQueueEntry>){qm.queue_depth_});
    HRUN_THALLIUM->RegisterRpc(
        *HRUN_WORK_ORCHESTRATOR->rpc_pool_,
        "RpcTaskSubmit", [this](
        const tl::request &req,
        tl::bulk &bulk,
        SegmentedTransfer &xfer) {
      this->RpcTaskSubmit(req, bulk, xfer);
    });
    submitters_.resize(max_lanes);
    completers_.resize(max_lanes);
    for (size_t i = 0; i < max_lanes; ++i) {
      submitters_[i] = HRUN_REMOTE_QUEUE->AsyncClientSubmit(
          task, task->task_node_ + 1,
          DomainId::GetLocal(), i);
      completers_[i] = HRUN_REMOTE_QUEUE->AsyncServerComplete(
          task, task->task_node_ + 1,
          DomainId::GetLocal(), i);
    }
    task->SetModuleComplete();
  }
  void MonitorConstruct(u32 mode, ConstructTask *task, RunContext &rctx) {
  }

  /** Destroy remote queue */
  void Destruct(DestructTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorDestruct(u32 mode, DestructTask *task, RunContext &rctx) {
  }

  /** Push operation called on client */
  void ClientPushSubmit(ClientPushSubmitTask *task, RunContext &rctx) {
    std::vector<DomainId> domain_ids = HRUN_RUNTIME->ResolveDomainId(
        task->domain_id_);
    BinaryOutputArchive<true> ar;
    task->ctx_.next_net_ = new RemoteInfo();
    RemoteInfo *remote = (RemoteInfo*)task->ctx_.next_net_;
    remote->rep_cnt_ = 0;
    remote->rep_max_ = domain_ids.size();
    remote->replicas_.resize(domain_ids.size());
    for (DomainId &domain_id  : domain_ids) {
      size_t lane_hash = std::hash<DomainId>{}(domain_id);
      submit_[lane_hash % submit_.size()].emplace(
          (TaskQueueEntry){domain_id, task});
    }
  }
  void MonitorClientPushSubmit(u32 mode,
                               ClientPushSubmitTask *task,
                               RunContext &rctx) {
  }

  /** Push operation called on client */
  void ClientSubmit(ClientSubmitTask *task, RunContext &rctx) {
    try {
      TaskQueueEntry entry;
      std::unordered_map<DomainId, BinaryOutputArchive<true>> entries;
      while (!submit_[rctx.lane_id_].pop(entry).IsNull()) {
        if (entries.find(entry.domain_) == entries.end()) {
          entries.emplace(entry.domain_, BinaryOutputArchive<true>());
        }
        BinaryOutputArchive<true> &ar = entries[entry.domain_];
        rctx.exec_->SaveStart(task->method_, ar, task);
      }

      for (auto it = entries.begin(); it != entries.end(); ++it) {
        SegmentedTransfer xfer = it->second.Get();
        xfer.ret_domain_ =
            DomainId::GetNode(HRUN_CLIENT->node_id_);
        HRUN_THALLIUM->SyncIoCall<int>((i32)it->first.id_,
                                       "RpcTaskSubmit",
                                       xfer,
                                       DT_SENDER_WRITE);
      }
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Worker {} caught an error: {}", HRUN_CLIENT->node_id_, id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Worker {} caught an exception: {}", HRUN_CLIENT->node_id_, id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Worker {} caught an unknown exception", HRUN_CLIENT->node_id_, id_);
    }
  }
  void MonitorClientSubmit(u32 mode,
                           ClientSubmitTask *task,
                           RunContext &rctx) {
  }

  /** Complete the task (on the remote node) */
  void ServerPushComplete(ServerPushCompleteTask *task,
                          RunContext &rctx) {
    RemoteInfo *remote = (RemoteInfo*)task->ctx_.prior_net_;
    size_t lane_hash = std::hash<DomainId>{}(remote->ret_domain_);
    complete_[lane_hash % complete_.size()].emplace((TaskQueueEntry){
      remote->ret_domain_, task
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
      TaskQueueEntry entry;
      std::unordered_map<DomainId, BinaryOutputArchive<false>> entries;
      while (!submit_[rctx.lane_id_].pop(entry).IsNull()) {
        if (entries.find(entry.domain_) == entries.end()) {
          entries.emplace(entry.domain_, BinaryOutputArchive<false>());
        }
        RemoteInfo *remote = (RemoteInfo*)task->ctx_.prior_net_;
        task->ctx_.task_addr_ = remote->task_addr_;
        BinaryOutputArchive<false> &ar = entries[entry.domain_];
        rctx.exec_->SaveEnd(task->method_, ar, task);
      }

      for (auto it = entries.begin(); it != entries.end(); ++it) {
        SegmentedTransfer xfer = it->second.Get();
        xfer.ret_domain_ =
            DomainId::GetNode(HRUN_CLIENT->node_id_);
        HRUN_THALLIUM->SyncIoCall<int>((i32)it->first.id_,
                                       "RpcTaskComplete",
                                       xfer,
                                       DT_SENDER_WRITE);
      }
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Worker {} caught an error: {}", HRUN_CLIENT->node_id_, id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Worker {} caught an exception: {}", HRUN_CLIENT->node_id_, id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Worker {} caught an unknown exception", HRUN_CLIENT->node_id_, id_);
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
      HILOG(kDebug, "(node {}) Received large message of size {})",
            HRUN_CLIENT->node_id_, xfer.size());
      HRUN_THALLIUM->IoCallServerWrite(req, bulk, xfer);
      BinaryInputArchive<true> ar(xfer);
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        hshm::Timer t;
        t.Resume();
        RemoteInfo *remote = new RemoteInfo();
        remote->ret_domain_ = xfer.ret_domain_;
        DeserializeTask(i, ar, xfer, remote);
        t.Pause();
        HILOG(kInfo, "(node {}) Submitted large message in {} usec",
              HRUN_CLIENT->node_id_, t.GetUsec());
      }
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Worker {} caught an error: {}", HRUN_CLIENT->node_id_, id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Worker {} caught an exception: {}", HRUN_CLIENT->node_id_, id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Worker {} caught an unknown exception", HRUN_CLIENT->node_id_, id_);
    }
    req.respond(0);
  }

  /** Push operation called at the remote server */
  void DeserializeTask(size_t task_off,
                       BinaryInputArchive<true> &ar,
                       SegmentedTransfer &xfer,
                       RemoteInfo *remote) {
    // Deserialize task
    TaskStateId state_id = xfer.tasks_[task_off].task_state_;
    u32 method = xfer.tasks_[task_off].method_;
    remote->task_addr_ = xfer.tasks_[task_off].task_addr_;
    TaskState *exec = HRUN_TASK_REGISTRY->GetTaskState(state_id);
    if (exec == nullptr) {
      HELOG(kFatal, "(node {}) Could not find the task state {}",
            HRUN_CLIENT->node_id_, state_id);
      return;
    }
    TaskPointer task_ptr = exec->LoadStart(method, ar);
    Task *orig_task = task_ptr.ptr_;
    orig_task = task_ptr.ptr_;
    orig_task->domain_id_ = DomainId::GetNode(HRUN_CLIENT->node_id_);

    // Unset task flags
    // NOTE(llogan): Remote tasks are executed to completion and
    // return values sent back to the remote host. This is
    // for things like long-running monitoring tasks.
    orig_task->UnsetStarted();
    orig_task->UnsetSignalComplete();
    orig_task->UnsetBlocked();
    orig_task->UnsetLongRunning();
    orig_task->UnsetRemote();
    orig_task->UnsetFireAndForget();
    orig_task->SetDataOwner();
    orig_task->SetSignalRemoteComplete();
    orig_task->task_flags_.SetBits(TASK_REMOTE_DEBUG_MARK);
    orig_task->ctx_.prior_net_ = remote;

    // Execute task
    MultiQueue *queue = HRUN_CLIENT->GetQueue(QueueId(state_id));
    queue->Emplace(orig_task->prio_, orig_task->lane_hash_, task_ptr.shm_);
    HILOG(kDebug,
          "(node {}) Submitting task (task_node={}, task_state={}/{}, state_name={}, method={}, size={}, lane_hash={})",
          HRUN_CLIENT->node_id_,
          orig_task->task_node_,
          orig_task->task_state_,
          state_id,
          exec->name_,
          method,
          xfer.size(),
          orig_task->lane_hash_);
  }

  /** Receive task completion */
  void RpcTaskComplete(const tl::request &req,
                       tl::bulk &bulk,
                       SegmentedTransfer &xfer) {
    try {
      HILOG(kDebug, "(node {}) Received large message of size {})",
            HRUN_CLIENT->node_id_, xfer.size());
      // Get task return values
      BinaryInputArchive<false> ar(xfer);
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        Task *task = (Task*)xfer.tasks_[i].task_addr_;
        TaskState *exec = HRUN_TASK_REGISTRY->GetTaskState(task->task_state_);
        if (exec == nullptr) {
          HELOG(kFatal, "(node {}) Could not find the task state {}",
                HRUN_CLIENT->node_id_, task->task_state_);
          return;
        }
        RemoteInfo *remote = (RemoteInfo*)task->ctx_.next_net_;
        if (remote->rep_max_ == 1) {
          exec->LoadEnd(task->method_, ar, task);
        } else {
          size_t rep_id = remote->rep_cnt_.fetch_add(1);
          remote->replicas_[rep_id] = exec->LoadReplicaEnd(task->method_, ar);
        }
      }
      xfer = ar.Get();
      HRUN_THALLIUM->IoCallServerWrite(req, bulk, xfer);
      // Check if all replicas are complete
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        Task *task = (Task*)xfer.tasks_[i].task_addr_;
        TaskState *exec = HRUN_TASK_REGISTRY->GetTaskState(task->task_state_);
        RemoteInfo *remote = (RemoteInfo*)task->ctx_.next_net_;
        if (remote->rep_cnt_ == remote->rep_max_) {
          if (remote->rep_max_ > 1) {
            exec->Monitor(MonitorMode::kReplicaAgg, task, task->ctx_);
          }
          for (size_t i = 0; i < remote->replicas_.size(); ++i) {
            Task *replica = remote->replicas_[i].ptr_;
            exec->Del(replica->method_, replica);
          }
          delete remote;
          Worker &worker = HRUN_WORK_ORCHESTRATOR->GetWorker(task->ctx_.worker_id_);
          LPointer<Task> ltask;
          ltask.ptr_ = task;
          ltask.shm_ = HERMES_MEMORY_MANAGER->Convert<Task, hipc::Pointer>(
              ltask.ptr_);
          worker.SignalComplete(ltask);
        }
      }
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Worker {} caught an error: {}", HRUN_CLIENT->node_id_, id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Worker {} caught an exception: {}", HRUN_CLIENT->node_id_, id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Worker {} caught an unknown exception", HRUN_CLIENT->node_id_, id_);
    }
    req.respond(0);
  }

 public:
#include "remote_queue/remote_queue_lib_exec.h"
};
}  // namespace chm

HRUN_TASK_CC(chm::remote_queue::Server, "remote_queue");
