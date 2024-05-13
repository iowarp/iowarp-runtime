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

#ifndef HRUN_INCLUDE_CHM_CLIENT_HRUN_SERVER_H_
#define HRUN_INCLUDE_CHM_CLIENT_HRUN_SERVER_H_

#include "chimaera/task_registry/task_registry.h"
#include "chimaera/work_orchestrator/work_orchestrator.h"
#include "chimaera/queue_manager/queue_manager_runtime.h"
#include "chimaera_admin/chimaera_admin.h"
#include "remote_queue/remote_queue.h"
#include "chimaera_client.h"
#include "manager.h"
#include "chimaera/network/rpc.h"
#include "chimaera/network/rpc_thallium.h"

// Singleton macros
#define CHM_RUNTIME hshm::Singleton<chm::Runtime>::GetInstance()
#define CHM_RUNTIME_T chm::Runtime*
#define CHM_REMOTE_QUEUE (&CHM_RUNTIME->remote_queue_)
#define CHM_THALLIUM (&CHM_RUNTIME->thallium_)
#define CHM_RPC (&CHM_RUNTIME->rpc_)

#define HRUN_RUNTIME CHM_RUNTIME
#define HRUN_RUNTIME_T CHM_RUNTIME_T
#define HRUN_REMOTE_QUEUE CHM_REMOTE_QUEUE
#define HRUN_THALLIUM CHM_THALLIUM
#define HRUN_RPC CHM_RPC

namespace chm {

class Runtime : public ConfigurationManager {
 public:
  int data_;
  TaskRegistry task_registry_;
  WorkOrchestrator work_orchestrator_;
  QueueManagerRuntime queue_manager_;
  remote_queue::Client remote_queue_;
  RpcContext rpc_;
  ThalliumRpc thallium_;
  bool remote_created_ = false;

 public:
  /** Default constructor */
  Runtime() = default;

  /** Create the server-side API */
  Runtime* Create(std::string server_config_path = "");

 private:
  /** Initialize */
  void ServerInit(std::string server_config_path);

 public:
  /** Initialize shared-memory between daemon and client */
  void InitSharedMemory();

  /** Finalize Hermes explicitly */
  void Finalize();

  /** Run the Hermes core Daemon */
  void RunDaemon();

  /** Stop the Hermes core Daemon */
  void StopDaemon();

  /** Get # of lanes from QueueManager */
  size_t GetNumLanes() {
    return queue_manager_.max_lanes_;
  }
};

}  // namespace chm

#endif  // HRUN_INCLUDE_CHM_CLIENT_HRUN_SERVER_H_
