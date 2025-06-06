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

#include "chimaera/config/config_server.h"

#include <hermes_shm/util/config_parse.h>
#include <hermes_shm/util/logging.h>
#include <string.h>
#include <yaml-cpp/yaml.h>

#include <ostream>

#include "chimaera/config/config.h"
#include "chimaera/config/config_server_default.h"

namespace chi::config {

/** parse work orchestrator info from YAML config */
void ServerConfig::ParseWorkOrchestrator(YAML::Node yaml_conf) {
  if (yaml_conf["cpus"]) {
    ClearParseVector<u32>(yaml_conf["cpus"], wo_.cpus_);
  }
  if (yaml_conf["reinforce_cpu"]) {
    wo_.reinforce_cpu_ = yaml_conf["reinforce_cpu"].as<u32>();
  }
  if (yaml_conf["monitor_gap"]) {
    wo_.monitor_gap_ = yaml_conf["monitor_gap"].as<size_t>();
  }
  if (yaml_conf["monitor_window"]) {
    wo_.monitor_window_ = yaml_conf["monitor_window"].as<size_t>();
  }
}

/** parse work orchestrator info from YAML config */
void ServerConfig::ParseQueueManager(YAML::Node yaml_conf) {
  if (yaml_conf["queue_depth"]) {
    queue_manager_.queue_depth_ = yaml_conf["queue_depth"].as<size_t>();
  }
  if (yaml_conf["proc_queue_depth"]) {
    queue_manager_.proc_queue_depth_ =
        yaml_conf["proc_queue_depth"].as<size_t>();
  }
  if (yaml_conf["comux_depth"]) {
    queue_manager_.comux_depth_ = yaml_conf["comux_depth"].as<size_t>();
  }
  if (yaml_conf["lane_depth"]) {
    queue_manager_.lane_depth_ = yaml_conf["lane_depth"].as<size_t>();
  }
  if (yaml_conf["shm_name"]) {
    queue_manager_.shm_name_ =
        hshm::ConfigParse::ExpandPath(yaml_conf["shm_name"].as<std::string>());
    queue_manager_.data_shm_name_ = queue_manager_.shm_name_ + "_data";
    queue_manager_.rdata_shm_name_ = queue_manager_.shm_name_ + "_rdata";
    queue_manager_.base_gpu_cpu_name_ = queue_manager_.shm_name_ + "_gpu_cpu";
    queue_manager_.base_gpu_data_name_ = queue_manager_.shm_name_ + "_gpu_data";
  }
  if (yaml_conf["shm_size"]) {
    queue_manager_.shm_size_ =
        hshm::ConfigParse::ParseSize(yaml_conf["shm_size"].as<std::string>());
  }
  if (yaml_conf["data_shm_size"]) {
    queue_manager_.data_shm_size_ = hshm::ConfigParse::ParseSize(
        yaml_conf["data_shm_size"].as<std::string>());
  }
  if (yaml_conf["rdata_shm_size"]) {
    queue_manager_.rdata_shm_size_ = hshm::ConfigParse::ParseSize(
        yaml_conf["rdata_shm_size"].as<std::string>());
  }
  if (yaml_conf["gpu_md_shm_size"]) {
    queue_manager_.gpu_md_shm_size_ = hshm::ConfigParse::ParseSize(
        yaml_conf["gpu_md_shm_size"].as<std::string>());
  }
  if (yaml_conf["gpu_data_shm_size"]) {
    queue_manager_.gpu_data_shm_size_ = hshm::ConfigParse::ParseSize(
        yaml_conf["gpu_data_shm_size"].as<std::string>());
  }
}

/** parse RPC information from YAML config */
void ServerConfig::ParseRpcInfo(YAML::Node yaml_conf) {
  std::string suffix;
  rpc_.host_names_.clear();
  if (yaml_conf["host_file"]) {
    rpc_.host_file_ =
        hshm::ConfigParse::ExpandPath(yaml_conf["host_file"].as<std::string>());
    if (rpc_.host_file_.size() > 0) {
      rpc_.host_names_ = hshm::ConfigParse::ParseHostfile(rpc_.host_file_);
    }
  }
  if (yaml_conf["host_names"] && rpc_.host_names_.size() == 0) {
    for (YAML::Node host_name_gen : yaml_conf["host_names"]) {
      std::string host_names = host_name_gen.as<std::string>();
      hshm::ConfigParse::ParseHostNameString(host_names, rpc_.host_names_);
    }
  }
  if (yaml_conf["domain"]) {
    rpc_.domain_ = yaml_conf["domain"].as<std::string>();
  }
  if (yaml_conf["protocol"]) {
    rpc_.protocol_ = yaml_conf["protocol"].as<std::string>();
  }
  if (yaml_conf["port"]) {
    rpc_.port_ = yaml_conf["port"].as<int>();
  }
  if (yaml_conf["cpus"]) {
    ClearParseVector<u32>(yaml_conf["cpus"], rpc_.cpus_);
    rpc_.num_threads_ = rpc_.cpus_.size();
  }
}

/** parse the YAML node */
void ServerConfig::ParseYAML(YAML::Node &yaml_conf) {
  if (yaml_conf["work_orchestrator"]) {
    ParseWorkOrchestrator(yaml_conf["work_orchestrator"]);
  }
  if (yaml_conf["queue_manager"]) {
    ParseQueueManager(yaml_conf["queue_manager"]);
  }
  if (yaml_conf["rpc"]) {
    ParseRpcInfo(yaml_conf["rpc"]);
  }
  if (yaml_conf["module_registry"]) {
    ParseVector<std::string>(yaml_conf["module_registry"], modules_);
  }
}

/** Load the default configuration */
void ServerConfig::LoadDefault() {
  LoadText(kChiServerDefaultConfigStr, false);
}

} // namespace chi::config
