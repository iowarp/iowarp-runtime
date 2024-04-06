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
  tl::bulk read_bulk_;
  SegmentedTransfer xfer_;
};

class Server : public TaskLib {
 public:
  std::unordered_map<u32, std::vector<Task*>> submit_;
  std::unordered_map<u32, std::vector<Task*>> complete_;

 public:
  Server() = default;

  /** Construct remote queue */
  void Construct(ConstructTask *task, RunContext &rctx) {
    HILOG(kInfo, "(node {}) Constructing remote queue (task_node={}, task_state={}, method={})",
          HRUN_CLIENT->node_id_, task->task_node_, task->task_state_, task->method_);
    HRUN_THALLIUM->RegisterRpc(
        *HRUN_WORK_ORCHESTRATOR->rpc_pool_,
        "RpcTaskSubmit", [this](
        const tl::request &req,
        tl::bulk &bulk,
        SegmentedTransfer &xfer) {
      this->RpcTaskSubmit(req, bulk, xfer);
    });
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
  void Push(PushTask *task, RunContext &rctx) {
    std::vector<DomainId> domain_ids = HRUN_RUNTIME->ResolveDomainId(
        task->domain_id_);
    BinaryOutputArchive<true> ar;
    task->ctx_.remote_ = new RemoteInfo();
    RemoteInfo *remote = (RemoteInfo*)task->ctx_.remote_;
    remote->rep_cnt_ = 0;
    remote->rep_max_ = domain_ids.size();
    remote->replicas_.resize(domain_ids.size());
    for (DomainId &domain_id  : domain_ids) {
      submit_[domain_id.id_].push_back(task);
    }
  }
  void MonitorPush(u32 mode, PushTask *task, RunContext &rctx) {
  }

  /** Push operation called on client */
  void Process(ProcessTask *task, RunContext &rctx) {
    try {
      for (const std::pair<u32, std::vector<Task*>> &sq : submit_) {
        BinaryOutputArchive<true> ar;
        rctx.exec_->SaveStart(task->method_, ar, task);
        SegmentedTransfer xfer = ar.Get();
        xfer.ret_domain_ =
            DomainId::GetNode(HRUN_CLIENT->node_id_);
        HRUN_THALLIUM->SyncIoCall<int>((i32)sq.first,
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
  void MonitorProcess(u32 mode, ProcessTask *task, RunContext &rctx) {
  }

  /** Complete the task (on the remote node) */
  void PushComplete(PushCompleteTask *task, RunContext &rctx) {
  }
  void MonitorPushComplete(u32 mode, PushCompleteTask *task, RunContext &rctx) {
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
        remote->xfer_ = xfer;
        RpcExec(i, ar, xfer, remote);
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
  void RpcExec(size_t task_off,
               BinaryInputArchive<true> &ar,
               SegmentedTransfer &xfer,
               RemoteInfo *remote) {
    // Deserialize task
    TaskStateId state_id = xfer.tasks_[task_off].first;
    u32 method = xfer.tasks_[task_off].second;
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
    orig_task->UnsetDataOwner();
    orig_task->SetSignalRemoteComplete();
    orig_task->task_flags_.SetBits(TASK_REMOTE_DEBUG_MARK);
    orig_task->ctx_.remote_ = remote;

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

 public:
#include "remote_queue/remote_queue_lib_exec.h"
};
}  // namespace chm

HRUN_TASK_CC(chm::remote_queue::Server, "remote_queue");
