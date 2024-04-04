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

#ifndef HRUN_INCLUDE_HRUN_CLIENT_HRUN_SERVER_H_
#define HRUN_INCLUDE_HRUN_CLIENT_HRUN_SERVER_H_

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
#define HRUN_RUNTIME hshm::Singleton<chm::Runtime>::GetInstance()
#define HRUN_RUNTIME_T chm::Runtime*
#define HRUN_REMOTE_QUEUE (&HRUN_RUNTIME->remote_queue_)
#define HRUN_THALLIUM (&HRUN_RUNTIME->thallium_)
#define HRUN_RPC (&HRUN_RUNTIME->rpc_)

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

  /** Get the set of DomainIds */
  std::vector<DomainId> ResolveDomainId(const DomainId &domain_id);
};

}  // namespace chm

#endif  // HRUN_INCLUDE_HRUN_CLIENT_HRUN_SERVER_H_
