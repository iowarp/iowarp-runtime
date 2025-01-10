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

#ifndef CHI_INCLUDE_CHI_CLIENT_CHI_SERVER_H_
#define CHI_INCLUDE_CHI_CLIENT_CHI_SERVER_H_

#include "chimaera/module_registry/module_registry.h"
#include "chimaera/network/rpc.h"
#include "chimaera/network/rpc_thallium.h"
#include "chimaera/queue_manager/queue_manager_runtime.h"
#include "chimaera/work_orchestrator/work_orchestrator.h"
#include "chimaera_admin/chimaera_admin.h"
#include "chimaera_client_defn.h"
#include "manager.h"
#include "remote_queue/remote_queue.h"

// Singleton macros
#define CHI_RUNTIME hshm::Singleton<chi::Runtime>::GetInstance()
#define CHI_RUNTIME_T chi::Runtime*
#define CHI_REMOTE_QUEUE (&CHI_RUNTIME->remote_queue_)
#define CHI_THALLIUM (&CHI_RUNTIME->thallium_)

namespace chi {

class Runtime : public ConfigurationManager {
 public:
  int data_;
  remote_queue::Client remote_queue_;
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

  /** Initialize shared-memory between daemon and GPUs */
  void InitSharedMemoryGpu();

  /** Finalize Hermes explicitly */
  void Finalize();

  /** Run the Hermes core Daemon */
  void RunDaemon();

  /** Stop the Hermes core Daemon */
  void StopDaemon();

  /** Get # of lanes from QueueManager */
  size_t GetMaxContainersPn() { return CHI_QM_RUNTIME->max_containers_pn_; }
};

}  // namespace chi

#endif  // CHI_INCLUDE_CHI_CLIENT_CHI_SERVER_H_
