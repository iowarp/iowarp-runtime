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

#ifndef CHI_SRC_CONFIG_SERVER_H_
#define CHI_SRC_CONFIG_SERVER_H_

#include "chimaera/chimaera_types.h"
#include "config.h"

namespace chi::config {

/**
 * Work orchestrator information defined in server config
 * */
struct WorkOrchestratorInfo {
  /** CPU bindings for workers */
  std::vector<u32> cpus_;
  /** CPU binding for reinforcement worker */
  u32 reinforce_cpu_;
  /** Monitoring gap */
  size_t monitor_gap_;
  /** Monitoring window */
  size_t monitor_window_;
};

/**
 * Queue manager information defined in server config
 * */
struct QueueManagerInfo {
  /** Maximum depth of IPC queues */
  u32 queue_depth_;
  /** Maximum depth of process queue */
  u32 proc_queue_depth_;
  /** Maximum number of lanes per IPC queue */
  u32 max_containers_pn_;
  /** Maximum number of allocatable IPC queues */
  u32 max_queues_;
  /** Shared memory region name */
  std::string shm_name_;
  /** Shared memory region size */
  size_t shm_size_;
  /** Shared memory data region name */
  std::string data_shm_name_;
  /** Shared memory runtime data region name */
  std::string rdata_shm_name_;
  /** Client data shared memory region size */
  size_t data_shm_size_;
  /** Runtime data shared memory region size */
  size_t rdata_shm_size_;

  HSHM_HOST_FUN
  QueueManagerInfo() = default;

  HSHM_HOST_FUN
  ~QueueManagerInfo() = default;
};

/**
 * RPC information defined in server config
 * */
struct RpcInfo {
  /** The name of a file that contains host names, 1 per line */
  std::string host_file_;
  /** The parsed hostnames from the hermes conf */
  std::vector<std::string> host_names_;
  /** The RPC protocol to be used. */
  std::string protocol_;
  /** The RPC domain name for verbs transport. */
  std::string domain_;
  /** The RPC port number. */
  int port_;
  /** Number of RPC threads */
  int num_threads_;
  /** CPU bindings for workers */
  std::vector<u32> cpus_;
};

/**
 * System configuration for Hermes
 */
class ServerConfig : public BaseConfig {
 public:
  /** Work orchestrator info */
  WorkOrchestratorInfo wo_;
  /** Queue manager info */
  QueueManagerInfo queue_manager_;
  /** The RPC information */
  RpcInfo rpc_;
  /** Bootstrap task registry */
  std::vector<std::string> modules_;

 public:
  ServerConfig() = default;
  void LoadDefault();

 private:
  void ParseYAML(YAML::Node &yaml_conf);
  void ParseWorkOrchestrator(YAML::Node yaml_conf);
  void ParseQueueManager(YAML::Node yaml_conf);
  void ParseRpcInfo(YAML::Node yaml_conf);
};

}  // namespace chi::config

namespace chi {
using ServerConfig = config::ServerConfig;
}  // namespace chi

#endif  // CHI_SRC_CONFIG_SERVER_H_
