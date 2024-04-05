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

namespace thallium {

/** Serialize I/O type enum */
// SERIALIZE_ENUM(chm::IoType);

}  // namespace thallium

namespace chm::remote_queue {

struct RemoteInfo {
  int replica_;
  std::atomic<u32> rep_cnt_;
  u32 rep_max_;
  size_t task_addr_;
  DomainId ret_domain_;
  LPointer<char> data_;
  std::vector<LPointer<Task>> replicas_;
};

class Server : public TaskLib {
 public:

 public:
  Server() = default;

  /** Construct remote queue */
  void Construct(ConstructTask *task, RunContext &rctx) {
    HILOG(kInfo, "(node {}) Constructing remote queue (task_node={}, task_state={}, method={})",
          HRUN_CLIENT->node_id_, task->task_node_, task->task_state_, task->method_);
    HRUN_THALLIUM->RegisterRpc(
        *HRUN_WORK_ORCHESTRATOR->rpc_pool_,
        "RpcPushSmall", [this](
        const tl::request &req,
        TaskStateId state_id,
        u32 method,
        size_t task_addr,
        int replica,
        const DomainId &domain_id,
        std::string &params) {
      this->RpcPushSmall(req, state_id, method,
                         task_addr, replica, domain_id, params);
    });
    HRUN_THALLIUM->RegisterRpc(
        *HRUN_WORK_ORCHESTRATOR->rpc_pool_,
        "RpcPushBulk", [this](
        const tl::request &req,
        const tl::bulk &bulk,
        TaskStateId state_id,
        u32 method,
        size_t task_addr,
        int replica,
        const DomainId &domain_id,
        std::string &params,
        size_t data_size,
        IoType io_type) {
      this->RpcPushBulk(req, state_id, method,
                        task_addr, replica, domain_id,
                        params, bulk, data_size, io_type);
    });
    HRUN_THALLIUM->RegisterRpc(
        *HRUN_WORK_ORCHESTRATOR->rpc_pool_,
        "RpcClientHandlePushReplicaOutput", [this](
        const tl::request &req,
        size_t task_addr,
        int replica,
        std::string &ret) {
      this->RpcClientHandlePushReplicaOutput(req, task_addr, replica, ret);
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
    BinaryOutputArchive<true> ar(
        DomainId::GetNode(HRUN_CLIENT->node_id_));
    std::vector<DataTransfer> xfer = rctx.exec_->SaveStart(
        task->method_, ar, task);
    task->ctx_.remote_ = new RemoteInfo();
    RemoteInfo *remote = (RemoteInfo*)task->ctx_.remote_;
    remote->rep_cnt_ = 0;
    remote->rep_max_ = domain_ids.size();
    remote->replicas_.resize(domain_ids.size());
    try {
      switch (xfer.size()) {
        case 1: {
          SyncClientSmallPush(xfer, domain_ids,
                              rctx.exec_, task);
          break;
        }
        case 2: {
          SyncClientIoPush(xfer, domain_ids,
                           rctx.exec_, task);
          break;
        }
        default: {
          HELOG(kFatal,
                "The task {}/{} does not support remote calls",
                task->task_state_,
                task->method_);
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
  void MonitorPush(u32 mode, PushTask *task, RunContext &rctx) {
  }

  /** Push operation called on client */
  void Process(ProcessTask *task, RunContext &rctx) {
  }
  void MonitorProcess(u32 mode, ProcessTask *task, RunContext &rctx) {
  }

 private:
  /** Sync Push for small message */
  void SyncClientSmallPush(std::vector<DataTransfer> &xfer,
                           std::vector<DomainId> &domain_ids,
                           TaskState *exec,
                           PushTask *task) {
    TaskStateId state_id = exec->id_;
    int method = task->method_;
    std::string params = std::string((char *) xfer[0].data_, xfer[0].data_size_);
    hshm::Timer t;
    t.Resume();
    for (int replica = 0; replica < domain_ids.size(); ++replica) {
      DomainId my_domain = DomainId::GetNode(HRUN_CLIENT->node_id_);
      DomainId domain_id = domain_ids[replica];
      HRUN_THALLIUM->SyncCall<int>(domain_id.id_,
                                   "RpcPushSmall",
                                   state_id,
                                   method,
                                   (size_t) task,
                                   replica,
                                   my_domain,
                                   params);
    }
    t.Pause();
    HILOG(kInfo, "(node {}) Pushed small message to {} replicas in {} usec",
          HRUN_CLIENT->node_id_, domain_ids.size(), t.GetUsec());
  }

  /** Sync Push for I/O message */
  void SyncClientIoPush(std::vector<DataTransfer> &xfer,
                        std::vector<DomainId> &domain_ids,
                        TaskState *exec,
                        PushTask *task) {
    std::string params = std::string((char *) xfer[1].data_, xfer[1].data_size_);
    IoType io_type = IoType::kRead;
    if (xfer[0].flags_.Any(DT_RECEIVER_READ)) {
      io_type = IoType::kWrite;
    }
    TaskStateId state_id = exec->id_;
    int method = task->method_;
    for (int replica = 0; replica < domain_ids.size(); ++replica) {
      DomainId my_domain = DomainId::GetNode(HRUN_CLIENT->node_id_);
      DomainId domain_id = domain_ids[replica];
      char *data = (char*)xfer[0].data_;
      size_t data_size = xfer[0].data_size_;
      if (data_size > 0) {
        HRUN_THALLIUM->SyncIoCall<int>(domain_id.id_,
                                       "RpcPushBulk",
                                       io_type,
                                       data,
                                       data_size,
                                       state_id,
                                       method,
                                       (size_t) task,
                                       replica,
                                       my_domain,
                                       params,
                                       data_size,
                                       io_type);
      } else {
        HELOG(kFatal, "(IO) Thallium can't handle 0-sized I/O")
      }
    }
    // task->rep_ = task->num_reps_;
  }

  /** The RPC for processing a small message */
  void RpcPushSmall(const tl::request &req,
                    TaskStateId state_id,
                    u32 method,
                    size_t task_addr,
                    int replica,
                    const DomainId &ret_domain,
                    std::string &params) {

    // Create the input data transfer object
    try {
      hshm::Timer t;
      t.Resume();
      std::vector<DataTransfer> xfer(1);
      xfer[0].data_ = params.data();
      xfer[0].data_size_ = params.size();
      HILOG(kDebug, "(node {}) Received small message of size {} "
                    "(task_state={}, method={})",
            HRUN_CLIENT->node_id_,
            xfer[0].data_size_, state_id, method);

      // Process the message
      TaskState *exec;
      Task *orig_task;
      LPointer<char> data;
      data.ptr_ = nullptr;
      RpcExec(req, state_id, method, task_addr, replica, ret_domain,
              xfer, data, orig_task, exec);
      t.Pause();
      HILOG(kInfo, "(node {}) Processed small message in {} usec",
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

  /** The RPC for processing a message with data */
  void RpcPushBulk(const tl::request &req,
                   TaskStateId state_id,
                   u32 method,
                   size_t task_addr,
                   int replica,
                   const DomainId &ret_domain,
                   std::string &params,
                   const tl::bulk &bulk,
                   size_t data_size,
                   IoType io_type) {
    LPointer<char> data;
    data.ptr_ = nullptr;
    try {
      data = HRUN_CLIENT->AllocateBufferServer<TASK_YIELD_ABT>(data_size);

      // Create the input data transfer object
      std::vector<DataTransfer> xfer(2);
      xfer[0].data_ = data.ptr_;
      xfer[0].data_size_ = data_size;
      xfer[1].data_ = params.data();
      xfer[1].data_size_ = params.size();

      HILOG(kDebug, "(node {}) Received large message of size {} "
                    "(task_state={}, method={})",
            HRUN_CLIENT->node_id_, xfer[0].data_size_, state_id, method);

      // Process the message
      if (io_type == IoType::kWrite) {
        HRUN_THALLIUM->IoCallServer(req, bulk, io_type, data.ptr_, data_size);
      }
      TaskState *exec;
      Task *orig_task;
      RpcExec(req, state_id, method, task_addr, replica, ret_domain,
              xfer, data, orig_task, exec);
      if (io_type == IoType::kRead) {
        HRUN_THALLIUM->IoCallServer(req, bulk, io_type, data.ptr_, data_size);
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
  void RpcExec(const tl::request &req,
               const TaskStateId &state_id,
               u32 method,
               size_t task_addr,
               int replica,
               const DomainId &ret_domain,
               std::vector<DataTransfer> &xfer,
               LPointer<char> &data,
               Task *&orig_task, TaskState *&exec) {
    size_t data_size = xfer[0].data_size_;
    BinaryInputArchive<true> ar(xfer);

    // Deserialize task
    exec = HRUN_TASK_REGISTRY->GetTaskState(state_id);
    if (exec == nullptr) {
      HELOG(kFatal, "(node {}) Could not find the task state {}",
            HRUN_CLIENT->node_id_, state_id);
      return;
    }
    TaskPointer task_ptr = exec->LoadStart(method, ar);
    orig_task = task_ptr.ptr_;
    orig_task->domain_id_ = DomainId::GetNode(HRUN_CLIENT->node_id_);

    // Unset task flags
    // NOTE(llogan): Remote tasks are executed to completion and
    // return values sent back to the remote host. This is
    // for things like long-running monitoring tasks.
    orig_task->SetFireAndForget();
    orig_task->UnsetStarted();
    orig_task->UnsetSignalComplete();
    orig_task->UnsetBlocked();
    orig_task->UnsetDataOwner();
    orig_task->UnsetLongRunning();
    orig_task->UnsetRemote();
    orig_task->SetSignalRemoteComplete();
    orig_task->task_flags_.SetBits(TASK_REMOTE_DEBUG_MARK);

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
          data_size,
          orig_task->lane_hash_);

    // Construct remote return information
    orig_task->ctx_.remote_ = new RemoteInfo();
    RemoteInfo *remote = (RemoteInfo*)orig_task->ctx_.remote_;
    remote->ret_domain_ = ret_domain;
    remote->replica_ = replica;
    remote->data_ = data;
    remote->task_addr_ = task_addr;
  }

  /** Complete the task (on the remote node) */
  void PushComplete(PushCompleteTask *task, RunContext &rctx) {
    RemoteInfo *remote = (RemoteInfo*)task->ctx_.remote_;
    TaskState *exec = rctx.exec_;
    RpcComplete(task->method_, task, exec,
                remote->task_addr_, remote->replica_,
                remote->ret_domain_, remote->data_);
    delete remote;
    task->SetModuleComplete();
  }
  void MonitorPushComplete(u32 mode, PushCompleteTask *task, RunContext &rctx) {
  }

  /** Complete a wait task */
  void RpcComplete(u32 method, Task *orig_task,
                   TaskState *exec,
                   size_t task_addr, int replica,
                   const DomainId &ret_domain,
                   LPointer<char> &data) {
    if (data.ptr_ != nullptr) {
      HRUN_CLIENT->FreeBuffer(data);
    }
    HILOG(kInfo, "(node {}) Returning replica output of task (task_node={}, task_state={}, method={})"
                 " to remote domain {}",
          HRUN_CLIENT->node_id_,
          orig_task->task_node_,
          orig_task->task_state_,
          method,
          ret_domain.id_);
    BinaryOutputArchive<false> ar(DomainId::GetNode(HRUN_CLIENT->node_id_));
    std::vector<DataTransfer> out_xfer = exec->SaveEnd(method, ar, orig_task);
    std::string ret;
    if (out_xfer.size() > 0 && out_xfer[0].data_size_ > 0) {
      ret = std::string((char *) out_xfer[0].data_, out_xfer[0].data_size_);
    }
    HRUN_THALLIUM->SyncCall<int>(ret_domain.id_,
                                  "RpcClientHandlePushReplicaOutput",
                                  task_addr,
                                  replica,
                                  ret);
  }

  /** Handle return of RpcComplete */
  void RpcClientHandlePushReplicaOutput(const tl::request &req,
                                        size_t task_addr,
                                        int replica,
                                        std::string &ret) {
    ClientHandlePushReplicaOutput(replica, ret, (PushTask *) task_addr);
    req.respond(0);
  }

  /** Handle output from replica PUSH */
  void ClientHandlePushReplicaOutput(int replica,
                                     std::string &ret,
                                     PushTask *task) {
    HILOG(kDebug, "(node {}) Received replica output for task "
                  "(task_node={}, task_state={}, method={})",
          HRUN_CLIENT->node_id_,
          task->task_node_,
          task->task_state_,
          task->method_);
    try {
      std::vector<DataTransfer> xfer(1);
      xfer[0].data_ = ret.data();
      xfer[0].data_size_ = ret.size();
      BinaryInputArchive<false> ar(xfer);
      TaskState *exec = HRUN_TASK_REGISTRY->GetTaskState(
          task->task_state_);
      RemoteInfo *remote = (RemoteInfo*)task->ctx_.remote_;
      if (remote->rep_max_ == 1) {
        exec->LoadEnd(task->method_, ar, task);
      } else {
        remote->replicas_[replica] =
            exec->LoadReplicaEnd(task->method_, ar);
      }
      remote->rep_cnt_ += 1;
      HILOG(kDebug, "(node {}) Handled replica output for task "
                    "(task_node={}, task_state={}, method={}, "
                    "rep={}, num_reps={})",
            HRUN_CLIENT->node_id_,
            task->task_node_,
            task->task_state_,
            task->method_,
            remote->rep_cnt_.load() + 1,
            remote->rep_max_);
      if (remote->rep_cnt_.load() == remote->rep_max_) {
        if (remote->rep_max_ > 1) {
          exec->Monitor(MonitorMode::kReplicaAgg, task, task->ctx_);
          for (LPointer<Task> &replica : remote->replicas_) {
            if (replica.ptr_ != nullptr) {
              exec->Del(task->method_, replica.ptr_);
            }
          }
        }
        delete remote;
        if (!task->IsLongRunning()) {
          task->SetModuleComplete();
        }
        Worker &worker = HRUN_WORK_ORCHESTRATOR->GetWorker(
            task->ctx_.worker_id_);
        LPointer<Task> task_ptr;
        task_ptr.ptr_ = task;
        task_ptr.shm_ = HERMES_MEMORY_MANAGER->Convert(task);
        worker.SignalComplete(task_ptr);
      }
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Caught an error: {}",
            HRUN_CLIENT->node_id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Caught an exception: {}",
            HRUN_CLIENT->node_id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Caught an unknown exception",
            HRUN_CLIENT->node_id_);
    }
  }

 public:
#include "remote_queue/remote_queue_lib_exec.h"
};
}  // namespace chm

HRUN_TASK_CC(chm::remote_queue::Server, "remote_queue");
