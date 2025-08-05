#ifndef CHI_CONFIG_MANAGER_H_
#define CHI_CONFIG_MANAGER_H_

#include <hermes_shm/util/config_parse.h>
#include <hermes_shm/util/singleton.h>
#include <vector>
#include <string>
#include <unistd.h>
#include "../chimaera_types.h"

namespace chi {

struct WorkOrchestratorConfig {
  std::vector<int> cpus_;
  int reinforce_cpu_;
  double monitor_window_;
  double monitor_gap_;

  void LoadDefault() {
    cpus_ = {0, 1, 2, 3};
    reinforce_cpu_ = 3;
    monitor_window_ = 1.0;
    monitor_gap_ = 5.0;
  }

  void ParseYAML(YAML::Node &yaml_conf) {
    if (yaml_conf["cpus"]) {
      cpus_.clear();
      for (auto val_node : yaml_conf["cpus"]) {
        cpus_.emplace_back(val_node.as<int>());
      }
    }
    if (yaml_conf["reinforce_cpu"]) {
      reinforce_cpu_ = yaml_conf["reinforce_cpu"].as<int>();
    }
    if (yaml_conf["monitor_window"]) {
      monitor_window_ = yaml_conf["monitor_window"].as<double>();
    }
    if (yaml_conf["monitor_gap"]) {
      monitor_gap_ = yaml_conf["monitor_gap"].as<double>();
    }
  }
};

struct QueueManagerConfig {
  u32 proc_queue_depth_;
  u32 queue_depth_;
  u32 comux_depth_;
  u32 lane_depth_;
  std::string shm_name_;
  hshm::u64 shm_size_;

  void LoadDefault() {
    proc_queue_depth_ = 8192;
    queue_depth_ = 100000;
    comux_depth_ = 256;
    lane_depth_ = 8192;
    shm_name_ = "chimaera_shm";
    shm_size_ = hshm::Unit<hshm::u64>::Gigabytes(1);
  }

  void ParseYAML(YAML::Node &yaml_conf) {
    if (yaml_conf["proc_queue_depth"]) {
      proc_queue_depth_ = yaml_conf["proc_queue_depth"].as<u32>();
    }
    if (yaml_conf["queue_depth"]) {
      queue_depth_ = yaml_conf["queue_depth"].as<u32>();
    }
    if (yaml_conf["comux_depth"]) {
      comux_depth_ = yaml_conf["comux_depth"].as<u32>();
    }
    if (yaml_conf["lane_depth"]) {
      lane_depth_ = yaml_conf["lane_depth"].as<u32>();
    }
    if (yaml_conf["shm_name"]) {
      shm_name_ = yaml_conf["shm_name"].as<std::string>();
    }
    if (yaml_conf["shm_size"]) {
      std::string size_str = yaml_conf["shm_size"].as<std::string>();
      shm_size_ = hshm::ConfigParse::ParseSize(size_str);
    }
  }
};

struct RpcConfig {
  std::string host_file_;
  std::vector<std::string> host_names_;
  std::string protocol_;
  std::string domain_;
  int port_;
  int node_id_;  // ID of this node in the cluster
  std::vector<std::string> resolved_hosts_;  // Resolved from hostfile or host_names

  void LoadDefault() {
    host_file_ = "";
    host_names_ = {"localhost"};
    protocol_ = "ofi+sockets";
    domain_ = "";
    port_ = 8080;
    node_id_ = 0;
    resolved_hosts_.clear();
  }

  void ParseYAML(YAML::Node &yaml_conf) {
    if (yaml_conf["host_file"]) {
      host_file_ = yaml_conf["host_file"].as<std::string>();
    }
    if (yaml_conf["host_names"]) {
      host_names_.clear();
      for (auto val_node : yaml_conf["host_names"]) {
        host_names_.emplace_back(val_node.as<std::string>());
      }
    }
    if (yaml_conf["protocol"]) {
      protocol_ = yaml_conf["protocol"].as<std::string>();
    }
    if (yaml_conf["domain"]) {
      domain_ = yaml_conf["domain"].as<std::string>();
    }
    if (yaml_conf["port"]) {
      port_ = yaml_conf["port"].as<int>();
    }
    if (yaml_conf["node_id"]) {
      node_id_ = yaml_conf["node_id"].as<int>();
    }
    
    // Resolve hosts from hostfile or host_names
    ResolveHosts();
  }

private:
  void ResolveHosts() {
    resolved_hosts_.clear();
    
    if (!host_file_.empty()) {
      // Use hostfile if specified
      try {
        resolved_hosts_ = hshm::ConfigParse::ParseHostfile(host_file_);
        HILOG(kInfo, "Loaded {} hosts from hostfile: {}", resolved_hosts_.size(), host_file_);
      } catch (const std::exception& e) {
        HELOG(kError, "Failed to parse hostfile {}: {}", host_file_, e.what());
        // Fallback to host_names
        resolved_hosts_ = host_names_;
      }
    } else {
      // Use host_names directly
      resolved_hosts_ = host_names_;
    }
    
    // Determine this node's hostname for node_id resolution
    if (node_id_ < 0 || node_id_ >= static_cast<int>(resolved_hosts_.size())) {
      // Try to auto-detect node_id by hostname
      char hostname[256];
      gethostname(hostname, sizeof(hostname));
      std::string current_hostname(hostname);
      
      for (size_t i = 0; i < resolved_hosts_.size(); ++i) {
        if (resolved_hosts_[i] == current_hostname || 
            resolved_hosts_[i] == "localhost" || 
            resolved_hosts_[i] == "127.0.0.1") {
          node_id_ = static_cast<int>(i);
          break;
        }
      }
      
      if (node_id_ < 0 || node_id_ >= static_cast<int>(resolved_hosts_.size())) {
        node_id_ = 0;  // Default to first node
        HELOG(kWarning, "Could not determine node ID, defaulting to 0");
      }
    }
    
    HILOG(kInfo, "Resolved node ID {} from {} total hosts", node_id_, resolved_hosts_.size());
  }

public:
  const std::vector<std::string>& GetResolvedHosts() const {
    return resolved_hosts_;
  }
  
  std::string GetCurrentHostname() const {
    if (node_id_ >= 0 && node_id_ < static_cast<int>(resolved_hosts_.size())) {
      return resolved_hosts_[node_id_];
    }
    return "localhost";
  }
};

class ChimaeraConfig : public hshm::BaseConfig {
public:
  WorkOrchestratorConfig work_orchestrator_;
  QueueManagerConfig queue_manager_;
  RpcConfig rpc_;
  std::vector<std::string> module_registry_;

  void LoadDefault() override {
    work_orchestrator_.LoadDefault();
    queue_manager_.LoadDefault();
    rpc_.LoadDefault();
    module_registry_.clear();
  }

private:
  void ParseYAML(YAML::Node &yaml_conf) override {
    if (yaml_conf["work_orchestrator"]) {
      work_orchestrator_.ParseYAML(yaml_conf["work_orchestrator"]);
    }
    if (yaml_conf["queue_manager"]) {
      queue_manager_.ParseYAML(yaml_conf["queue_manager"]);
    }
    if (yaml_conf["rpc"]) {
      rpc_.ParseYAML(yaml_conf["rpc"]);
    }
    if (yaml_conf["module_registry"]) {
      ClearParseVector(yaml_conf["module_registry"], module_registry_);
    }
  }
};

class ConfigurationManager {
private:
  ChimaeraConfig config_;

public:
  ConfigurationManager() = default;

  void LoadConfigFromFile(const std::string &config_path) {
    config_.LoadFromFile(config_path);
  }

  void LoadConfigFromEnv() {
    const char* config_path = std::getenv("CHI_SERVER_CONF");
    if (config_path) {
      LoadConfigFromFile(config_path);
    } else {
      config_.LoadDefault();
    }
  }

  const ChimaeraConfig& GetConfig() const {
    return config_;
  }

  const WorkOrchestratorConfig& GetWorkOrchestratorConfig() const {
    return config_.work_orchestrator_;
  }

  const QueueManagerConfig& GetQueueManagerConfig() const {
    return config_.queue_manager_;
  }

  const RpcConfig& GetRpcConfig() const {
    return config_.rpc_;
  }

  const std::vector<std::string>& GetModuleRegistry() const {
    return config_.module_registry_;
  }
};

}  // namespace chi

HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_H(chi::ConfigurationManager, chiConfigurationManager);
#define CHI_CONFIG_MANAGER \
  HSHM_GET_GLOBAL_CROSS_PTR_VAR(chi::ConfigurationManager, chiConfigurationManager)
#define CHI_CONFIG_MANAGER_T chi::ConfigurationManager*

#endif  // CHI_CONFIG_MANAGER_H_