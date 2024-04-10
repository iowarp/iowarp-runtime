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
  void Create(CreateTask *task, RunContext &rctx) {
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
    HRUN_THALLIUM->RegisterRpc(
        *HRUN_WORK_ORCHESTRATOR->rpc_pool_,
        "RpcTaskComplete", [this](
            const tl::request &req,
            tl::bulk &bulk,
            SegmentedTransfer &xfer) {
          this->RpcTaskComplete(req, bulk, xfer);
        });
    submitters_.resize(max_lanes);
    completers_.resize(max_lanes);
    HRUN_REMOTE_QUEUE->Init(id_);
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
  void MonitorCreate(u32 mode, CreateTask *task, RunContext &rctx) {
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
    if (domain_ids.size() == 0) {
      task->SetModuleComplete();
      return;
    }
    BinaryOutputArchive<true> ar;
    task->ctx_.next_net_ = new RemoteInfo();
    RemoteInfo *remote = (RemoteInfo*)task->ctx_.next_net_;
    remote->rep_cnt_ = 0;
    remote->rep_complete_ = 0;
    remote->rep_max_ = domain_ids.size();
    remote->replicas_.resize(domain_ids.size());
    for (DomainId &domain_id  : domain_ids) {
      size_t lane_hash = std::hash<DomainId>{}(domain_id);
      submit_[lane_hash % submit_.size()].emplace(
          (TaskQueueEntry){domain_id, task});
    }
    task->SetBlocked();
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
        HILOG(kDebug, "(node {}) Submitting task {} to domain {}",
              HRUN_CLIENT->node_id_, entry.task_->task_node_,
              entry.domain_.id_);
        if (entries.find(entry.domain_) == entries.end()) {
          entries.emplace(entry.domain_, BinaryOutputArchive<true>());
        }
        Task *orig_task = entry.task_;
        TaskState *exec = HRUN_TASK_REGISTRY->GetTaskState(
            orig_task->task_state_);
        if (exec == nullptr) {
          HELOG(kFatal, "(node {}) Could not find the task state {}",
                HRUN_CLIENT->node_id_, orig_task->task_state_);
          return;
        }
        BinaryOutputArchive<true> &ar = entries[entry.domain_];
        exec->SaveStart(orig_task->method_, ar, orig_task);
      }

      for (auto it = entries.begin(); it != entries.end(); ++it) {
        SegmentedTransfer xfer = it->second.Get();
        xfer.ret_domain_ =
            DomainId::GetNode(HRUN_CLIENT->node_id_);
        hshm::Timer t;
        t.Resume();
        HRUN_THALLIUM->SyncIoCall<int>((i32)it->first.id_,
                                       "RpcTaskSubmit",
                                       xfer,
                                       DT_SENDER_WRITE);
        t.Pause();
        HILOG(kInfo, "(node {}) Submitted tasks in {} usec",
              HRUN_CLIENT->node_id_, t.GetUsec());

        for (TaskSegment &task_seg : xfer.tasks_) {
          Task *orig_task = (Task*)task_seg.task_addr_;
          if (orig_task->IsFireAndForget()) {
            Worker &worker = HRUN_WORK_ORCHESTRATOR->GetWorker(
                orig_task->ctx_.worker_id_);
            orig_task->SetModuleComplete();
            worker.SignalUnblock(orig_task);
          }
        }
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
    switch (mode) {
      case MonitorMode::kFlushStat: {
        hshm::mpsc_queue<TaskQueueEntry> &submit = submit_[rctx.lane_id_];
        hshm::mpsc_queue<TaskQueueEntry> &complete = complete_[rctx.lane_id_];
        rctx.flush_->count_ += submit.GetSize() + complete.GetSize();
      }
    }
  }

  /** Complete the task (on the remote node) */
  void ServerPushComplete(ServerPushCompleteTask *task,
                          RunContext &rctx) {
    HILOG(kDebug, "(node {}) Task finished server-side {}",
          HRUN_CLIENT->node_id_, task->task_node_);
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
      // Serialize task completions
      TaskQueueEntry entry;
      std::unordered_map<DomainId, BinaryOutputArchive<false>> entries;
      size_t count = complete_[rctx.lane_id_].GetSize();
      std::vector<TaskQueueEntry> completed;
      completed.reserve(count);
      while (!complete_[rctx.lane_id_].pop(entry).IsNull()) {
        if (entries.find(entry.domain_) == entries.end()) {
          entries.emplace(entry.domain_, BinaryOutputArchive<false>());
        }
        Task *done_task = entry.task_;
        HILOG(kDebug, "(node {}) Sending completion for {} -> {}",
              HRUN_CLIENT->node_id_, done_task->task_node_,
              entry.domain_.id_);
        TaskState *exec =
            HRUN_TASK_REGISTRY->GetTaskState(done_task->task_state_);
        RemoteInfo *remote = (RemoteInfo*)done_task->ctx_.prior_net_;
        done_task->ctx_.task_addr_ = remote->task_addr_;
        BinaryOutputArchive<false> &ar = entries[entry.domain_];
        exec->SaveEnd(done_task->method_, ar, done_task);
        completed.emplace_back(entry);
      }

      // Do transfers
      for (auto it = entries.begin(); it != entries.end(); ++it) {
        SegmentedTransfer xfer = it->second.Get();
//        HILOG(kDebug, "(node {}) Sending completion of size {} to {}",
//              HRUN_CLIENT->node_id_, xfer.size(),
//              it->first.id_);
        HRUN_THALLIUM->SyncIoCall<int>((i32)it->first.id_,
                                       "RpcTaskComplete",
                                       xfer,
                                       DT_SENDER_WRITE);
      }

      // Cleanup the queue
      for (TaskQueueEntry &centry : completed) {
        Task *done_task = centry.task_;
        TaskState *exec =
            HRUN_TASK_REGISTRY->GetTaskState(done_task->task_state_);
        HRUN_CLIENT->DelTask(exec, done_task);
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
//      HILOG(kDebug, "(node {}) Received submission of size {}",
//            HRUN_CLIENT->node_id_, xfer.size());
      HRUN_THALLIUM->IoCallServerWrite(req, bulk, xfer);
      BinaryInputArchive<true> ar(xfer);
      hshm::Timer t;
      t.Resume();
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        RemoteInfo *remote = new RemoteInfo();
        remote->ret_domain_ = xfer.ret_domain_;
        DeserializeTask(i, ar, xfer, remote);
      }
      t.Pause();
      HILOG(kInfo, "(node {}) Submitted tasks in {} usec",
            HRUN_CLIENT->node_id_, t.GetUsec());
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
    orig_task->UnsetSignalUnblock();
    orig_task->UnsetBlocked();
    orig_task->UnsetLongRunning();
    orig_task->UnsetRemote();
    orig_task->SetDataOwner();
//    orig_task->UnsetFireAndForget();
//    orig_task->SetSignalRemoteComplete();
    if (!orig_task->IsFireAndForget()) {
      orig_task->SetSignalRemoteComplete();
    }
    orig_task->task_flags_.SetBits(TASK_REMOTE_DEBUG_MARK);
    orig_task->ctx_.prior_net_ = remote;

    // Execute task
    MultiQueue *queue = HRUN_CLIENT->GetQueue(QueueId(state_id));
    HILOG(kDebug,
          "(node {}) Submitting task (task_node={}, task_state={}/{}, "
          "state_name={}, method={}, size={}, lane_hash={})",
          HRUN_CLIENT->node_id_,
          orig_task->task_node_,
          orig_task->task_state_,
          state_id,
          exec->name_,
          method,
          xfer.size(),
          orig_task->lane_hash_);
    queue->Emplace(orig_task->prio_,
                   orig_task->lane_hash_, task_ptr.shm_);
    HILOG(kDebug, "Finished submitting task {}", orig_task->task_node_);
  }

  /** Receive task completion */
  void RpcTaskComplete(const tl::request &req,
                       tl::bulk &bulk,
                       SegmentedTransfer &xfer) {
    try {
      HILOG(kDebug, "(node {}) Received completion of size {}",
            HRUN_CLIENT->node_id_, xfer.size());
      // Get task return values
      BinaryInputArchive<false> ar(xfer);
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        Task *orig_task = (Task*)xfer.tasks_[i].task_addr_;
        HILOG(kDebug, "(node {}) Deserializing return values for task {} (state {})",
              HRUN_CLIENT->node_id_, orig_task->task_node_, orig_task->task_state_);
        TaskState *exec = HRUN_TASK_REGISTRY->GetTaskState(
            orig_task->task_state_);
        if (exec == nullptr) {
          HELOG(kFatal, "(node {}) Could not find the task state {}",
                HRUN_CLIENT->node_id_, orig_task->task_state_);
          return;
        }
        RemoteInfo *remote = (RemoteInfo*)orig_task->ctx_.next_net_;
        size_t rep_id = remote->rep_cnt_.fetch_add(1);
        if (remote->rep_max_ == 1) {
          exec->LoadEnd(orig_task->method_, ar, orig_task);
        } else {
          LPointer<Task> replica = exec->LoadReplicaEnd(
              orig_task->method_, ar, orig_task);
          remote->replicas_[rep_id] = replica;
        }
      }
      HRUN_THALLIUM->IoCallServerWrite(req, bulk, xfer);
      // Check if all replicas are complete
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        Task *orig_task = (Task*)xfer.tasks_[i].task_addr_;
        RemoteInfo *remote = (RemoteInfo*)orig_task->ctx_.next_net_;
        size_t rep_id = remote->rep_complete_.fetch_add(1);
        size_t rep_max = remote->rep_max_;
        if (rep_id == rep_max - 1) {
          TaskState *exec = HRUN_TASK_REGISTRY->GetTaskState(
              orig_task->task_state_);
          if (remote->rep_max_ > 1) {
            exec->Monitor(MonitorMode::kReplicaAgg,
                          orig_task, orig_task->ctx_);
            for (rep_id = 0; rep_id < remote->replicas_.size(); ++rep_id) {
              Task *replica = remote->replicas_[rep_id].ptr_;
              HRUN_CLIENT->DelTask(exec, replica);
            }
          }
          delete remote;
          Worker &worker = HRUN_WORK_ORCHESTRATOR->GetWorker(orig_task->ctx_.worker_id_);
          HILOG(kDebug, "(node {}) Unblocking the task {} (state {})",
                HRUN_CLIENT->node_id_, orig_task->task_node_, orig_task->task_state_);
          if (!orig_task->IsLongRunning()) {
            orig_task->SetModuleComplete();
          }
          worker.SignalUnblock(orig_task);
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
