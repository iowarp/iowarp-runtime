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
      submitters_[i] = HRUN_REMOTE_QUEUE->AsyncClientSubmit(
          task, task->task_node_ + 1,
          DomainId::GetLocal(), i);
      completers_[i] = HRUN_REMOTE_QUEUE->AsyncServerComplete(
          task, task->task_node_ + 1,
          DomainId::GetLocal(), i);
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
          HRUN_CLIENT->node_id_, task->task_node_, task->task_state_, task->method_);
    if (lane_id_ == 0) {
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
      HRUN_REMOTE_QUEUE->Init(id_);
      QueueManagerInfo &qm = HRUN_QM_RUNTIME->config_->queue_manager_;
      shared_ = std::make_shared<SharedState>(
          task, qm.queue_depth_, 1);
    } else {
      auto *root = (Server*)rctx.exec_;
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

  /** Push operation called on client */
  void ClientPushSubmit(ClientPushSubmitTask *task, RunContext &rctx) {
    // Get domain IDs
    Task *orig_task = task->orig_task_;
    std::vector<DomainId> domain_ids = HRUN_RUNTIME->ResolveDomainId(
        orig_task->domain_id_);
    if (domain_ids.size() == 0) {
      task->SetModuleComplete();
      Worker::SignalUnblock(orig_task);
      return;
    }

    HILOG(kDebug, "ClientPushTask: {} ({}), Original Task: {} ({})",
          (size_t)task, task, (size_t)orig_task, orig_task);

    Task state_buf(0);
    orig_task->GetState(state_buf);
    std::vector<LPointer<Task>> replicas;
    replicas.reserve(domain_ids.size());
    // Replicate task
    bool deep = domain_ids.size() > 1;
    for (DomainId &domain_id : domain_ids) {
      TaskState *exec = HRUN_TASK_REGISTRY->GetTaskStateAny(
          orig_task->task_state_);
      LPointer<Task> replica;
      exec->CopyStart(orig_task->method_, orig_task, replica, deep);
      replica->ctx_.pending_to_ = task;
      size_t lane_hash = std::hash<DomainId>{}(domain_id);
      auto &submit = shared_->submit_;
      submit[lane_hash % submit.size()].emplace(
          (TaskQueueEntry) {domain_id, replica.ptr_});
      replicas.emplace_back(replica);
    }
    // Wait & combine replicas
    if (!state_buf.IsFireAndForget()) {
      // Wait
      task->Wait<TASK_YIELD_CO>(replicas, TASK_MODULE_COMPLETE);
      // Combine
      TaskState *exec = HRUN_TASK_REGISTRY->GetTaskStateAny(
          orig_task->task_state_);
      rctx.replicas_ = &replicas;
      // Free
      exec->Monitor(MonitorMode::kReplicaAgg, orig_task, rctx);
      HILOG(kDebug, "Replicas were waited for and completed");
      for (LPointer<Task> &replica : replicas) {
        HRUN_CLIENT->DelTask(exec, replica.ptr_);
      }
    }

    // Unblock original task
    if (!orig_task->IsLongRunning()) {
      orig_task->SetModuleComplete();
    }
    HILOG(kDebug, "Will unblock the task {} to worker {}",
          (size_t)orig_task, orig_task->ctx_.worker_id_);
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
      std::unordered_map<DomainId, BinaryOutputArchive<true>> entries;
      auto &submit = shared_->submit_;
      while (!submit[0].pop(entry).IsNull()) {
        HILOG(kDebug, "(node {}) Submitting task {} ({}) to domain {}",
              HRUN_CLIENT->node_id_, entry.task_->task_node_,
              (size_t)entry.task_,
              entry.domain_.GetId());
        if (entries.find(entry.domain_) == entries.end()) {
          entries.emplace(entry.domain_, BinaryOutputArchive<true>());
        }
        Task *orig_task = entry.task_;
        TaskState *exec = HRUN_TASK_REGISTRY->GetTaskStateAny(
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
        HRUN_THALLIUM->SyncIoCall<int>((i32)it->first.GetId(),
                                       "RpcTaskSubmit",
                                       xfer,
                                       DT_SENDER_WRITE);
        t.Pause();
        HILOG(kDebug, "(node {}) Submitted tasks in {} usec",
              HRUN_CLIENT->node_id_, t.GetUsec());

        for (TaskSegment &task_seg : xfer.tasks_) {
          Task *orig_task = (Task*)task_seg.task_addr_;
          if (orig_task->IsFireAndForget()) {
            orig_task->SetModuleComplete();
            Worker::SignalUnblock(orig_task);
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
          HRUN_CLIENT->node_id_, task->task_node_);
    if (task->ctx_.ret_task_addr_ == (size_t)task) {
      HILOG(kFatal, "This shouldn't happen ever");
    }
    DomainId ret_domain = task->ctx_.ret_domain_;
    size_t lane_hash = std::hash<DomainId>{}(ret_domain);
    auto &complete = shared_->complete_;
    complete[lane_hash % complete.size()].emplace((TaskQueueEntry){
        ret_domain, task
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
      auto &complete = shared_->complete_;
      size_t count = complete[0].GetSize();
      std::vector<TaskQueueEntry> completed;
      completed.reserve(count);
      while (!complete[0].pop(entry).IsNull()) {
        if (entries.find(entry.domain_) == entries.end()) {
          entries.emplace(entry.domain_, BinaryOutputArchive<false>());
        }
        Task *done_task = entry.task_;
        HILOG(kDebug, "(node {}) Sending completion for {} -> {}",
              HRUN_CLIENT->node_id_, done_task->task_node_,
              entry.domain_);
        TaskState *exec =
            HRUN_TASK_REGISTRY->GetTaskStateAny(done_task->task_state_);
        BinaryOutputArchive<false> &ar = entries[entry.domain_];
        exec->SaveEnd(done_task->method_, ar, done_task);
        completed.emplace_back(entry);
      }

      // Do transfers
      for (auto it = entries.begin(); it != entries.end(); ++it) {
        SegmentedTransfer xfer = it->second.Get();
//        HILOG(kDebug, "(node {}) Sending completion of size {} to {}",
//              HRUN_CLIENT->node_id_, xfer.size(),
//              it->first.GetId());
        HRUN_THALLIUM->SyncIoCall<int>((i32)it->first.GetId(),
                                       "RpcTaskComplete",
                                       xfer,
                                       DT_SENDER_WRITE);
      }

      // Cleanup the queue
//      for (TaskQueueEntry &centry : completed) {
//        Task *done_task = centry.task_;
//        TaskState *exec =
//            HRUN_TASK_REGISTRY->GetTaskState(done_task->task_state_);
//        HRUN_CLIENT->DelTask(exec, done_task);
//      }
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
        DeserializeTask(i, ar, xfer);
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
                       SegmentedTransfer &xfer) {
    // Deserialize task
    TaskStateId state_id = xfer.tasks_[task_off].task_state_;
    u32 method = xfer.tasks_[task_off].method_;
    TaskState *exec = HRUN_TASK_REGISTRY->GetTaskStateAny(state_id);
    if (exec == nullptr) {
      HELOG(kFatal, "(node {}) Could not find the task state {}",
            HRUN_CLIENT->node_id_, state_id);
      return;
    }
    TaskPointer task_ptr = exec->LoadStart(method, ar);
    Task *orig_task = task_ptr.ptr_;
    orig_task = task_ptr.ptr_;
    orig_task->domain_id_ = DomainId::GetNode(HRUN_CLIENT->node_id_);
    orig_task->ctx_.ret_task_addr_ = xfer.tasks_[task_off].task_addr_;
    orig_task->ctx_.ret_domain_ = xfer.ret_domain_;
    if (orig_task->ctx_.ret_task_addr_ == (size_t)orig_task) {
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
//    orig_task->UnsetFireAndForget();
//    orig_task->SetSignalRemoteComplete();
    if (!orig_task->IsFireAndForget()) {
      orig_task->UnsetLongRunning();
      orig_task->SetSignalRemoteComplete();
    }
    orig_task->task_flags_.SetBits(TASK_REMOTE_DEBUG_MARK);

    // Execute task
    MultiQueue *queue = HRUN_CLIENT->GetQueue(QueueId(state_id));
    HILOG(kDebug,
          "(node {}) Submitting task (addr={}, task_node={}, task_state={}/{}, "
          "state_name={}, method={}, size={}, lane_hash={})",
          HRUN_CLIENT->node_id_,
          (size_t)orig_task,
          orig_task->task_node_,
          orig_task->task_state_,
          state_id,
          exec->name_,
          method,
          xfer.size(),
          orig_task->GetLaneHash());
    queue->Emplace(orig_task->prio_,
                   orig_task->GetLaneHash(), task_ptr.shm_);
    HILOG(kDebug,
          "(node {}) Done submitting (task_node={}, task_state={}/{}, "
          "state_name={}, method={}, size={}, lane_hash={})",
          HRUN_CLIENT->node_id_,
          orig_task->task_node_,
          orig_task->task_state_,
          state_id,
          exec->name_,
          method,
          xfer.size(),
          orig_task->GetLaneHash());
  }

  /** Receive task completion */
  void RpcTaskComplete(const tl::request &req,
                       tl::bulk &bulk,
                       SegmentedTransfer &xfer) {
    try {
      HILOG(kDebug, "(node {}) Received completion of size {}",
            HRUN_CLIENT->node_id_, xfer.size());
      // Deserialize message parameters
      BinaryInputArchive<false> ar(xfer);
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        Task *orig_task = (Task*)xfer.tasks_[i].task_addr_;
        HILOG(kDebug, "(node {}) Deserializing return values for task {} (state {})",
              HRUN_CLIENT->node_id_, orig_task->task_node_, orig_task->task_state_);
        TaskState *exec = HRUN_TASK_REGISTRY->GetTaskStateAny(
            orig_task->task_state_);
        if (exec == nullptr) {
          HELOG(kFatal, "(node {}) Could not find the task state {}",
                HRUN_CLIENT->node_id_, orig_task->task_state_);
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
        Task *pending_to = orig_task->ctx_.pending_to_;
        if (pending_to->task_state_ != id_) {
          HELOG(kFatal, "This shouldn't happen ever");
        }
        HILOG(kDebug, "(node {}) Unblocking task {} to worker {}",
              HRUN_CLIENT->node_id_,
              (size_t)pending_to,
              pending_to->ctx_.worker_id_);
        Worker::SignalUnblock(pending_to);
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
