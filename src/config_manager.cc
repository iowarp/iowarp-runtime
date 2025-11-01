/**
 * Configuration manager implementation
 */

#include "chimaera/config_manager.h"
#include <cstdlib>

// Global pointer variable definition for Configuration manager singleton
HSHM_DEFINE_GLOBAL_PTR_VAR_CC(chi::ConfigManager, g_config_manager);

namespace chi {

// Constructor and destructor removed - handled by HSHM singleton pattern

bool ConfigManager::ClientInit() {
  if (is_initialized_) {
    return true;
  }

  // Get configuration file path from environment
  config_file_path_ = GetServerConfigPath();
  HILOG(kInfo, "Config at: {}", config_file_path_);

  // Load YAML configuration if path is provided
  if (!config_file_path_.empty()) {
    if (!LoadYaml(config_file_path_)) {
      HELOG(kError,
            "Warning: Failed to load configuration from {}, using defaults",
            config_file_path_);
    }
  }

  is_initialized_ = true;
  return true;
}

bool ConfigManager::ServerInit() {
  // Configuration is needed by both client and server, so same implementation
  return ClientInit();
}

bool ConfigManager::LoadYaml(const std::string &config_path) {
  try {
    // Use HSHM BaseConfig methods
    LoadFromFile(config_path, true);
    return true;
  } catch (const std::exception &e) {
    return false;
  }
}

std::string ConfigManager::GetServerConfigPath() const {
  const char *env_path = std::getenv("CHI_SERVER_CONF");
  return env_path ? std::string(env_path) : std::string();
}

u32 ConfigManager::GetWorkerThreadCount(ThreadType thread_type) const {
  switch (thread_type) {
  case kSchedWorker:
    return sched_workers_;
  case kProcessReaper:
    return process_reaper_workers_;
  default:
    return 0;
  }
}

size_t ConfigManager::GetMemorySegmentSize(MemorySegment segment) const {
  switch (segment) {
  case kMainSegment:
    return main_segment_size_;
  case kClientDataSegment:
    return client_data_segment_size_;
  case kRuntimeDataSegment:
    return runtime_data_segment_size_;
  default:
    return 0;
  }
}

u32 ConfigManager::GetPort() const { return port_; }

u32 ConfigManager::GetNeighborhoodSize() const { return neighborhood_size_; }

std::string
ConfigManager::GetSharedMemorySegmentName(MemorySegment segment) const {
  std::string segment_name;

  switch (segment) {
  case kMainSegment:
    segment_name = main_segment_name_;
    break;
  case kClientDataSegment:
    segment_name = client_data_segment_name_;
    break;
  case kRuntimeDataSegment:
    segment_name = runtime_data_segment_name_;
    break;
  default:
    return "";
  }

  // Use HSHM's ExpandPath to resolve environment variables
  return hshm::ConfigParse::ExpandPath(segment_name);
}

std::string ConfigManager::GetHostfilePath() const {
  if (hostfile_path_.empty()) {
    return "";
  }

  // Use HSHM's ExpandPath to resolve environment variables in hostfile path
  return hshm::ConfigParse::ExpandPath(hostfile_path_);
}

bool ConfigManager::IsValid() const { return is_initialized_; }

LaneMapPolicy ConfigManager::GetLaneMapPolicy() const {
  return lane_map_policy_;
}

void ConfigManager::LoadDefault() {
  // Set default configuration values
  sched_workers_ = 8;
  process_reaper_workers_ = 1;

  main_segment_size_ = 1024 * 1024 * 1024;        // 1GB
  client_data_segment_size_ = 512 * 1024 * 1024;  // 512MB
  runtime_data_segment_size_ = 512 * 1024 * 1024; // 512MB

  port_ = 5555;
  neighborhood_size_ = 32;

  // Set default shared memory segment names with environment variables
  main_segment_name_ = "chi_main_segment_${USER}";
  client_data_segment_name_ = "chi_client_data_segment_${USER}";
  runtime_data_segment_name_ = "chi_runtime_data_segment_${USER}";

  // Set default hostfile path (empty means no networking/distributed mode)
  hostfile_path_ = "";

  // Set default lane mapping policy
  lane_map_policy_ = LaneMapPolicy::kRoundRobin;
}

void ConfigManager::ParseYAML(YAML::Node &yaml_conf) {
  // Parse worker thread counts
  if (yaml_conf["workers"]) {
    auto workers = yaml_conf["workers"];
    if (workers["sched_threads"]) {
      sched_workers_ = workers["sched_threads"].as<u32>();
    }
    if (workers["process_reaper_threads"]) {
      process_reaper_workers_ = workers["process_reaper_threads"].as<u32>();
    }
  }

  // Parse memory segments
  if (yaml_conf["memory"]) {
    auto memory = yaml_conf["memory"];
    if (memory["main_segment_size"]) {
      main_segment_size_ = hshm::ConfigParse::ParseSize(
          memory["main_segment_size"].as<std::string>());
    }
    if (memory["client_data_segment_size"]) {
      client_data_segment_size_ = hshm::ConfigParse::ParseSize(
          memory["client_data_segment_size"].as<std::string>());
    }
    if (memory["runtime_data_segment_size"]) {
      runtime_data_segment_size_ = hshm::ConfigParse::ParseSize(
          memory["runtime_data_segment_size"].as<std::string>());
    }
  }

  // Parse networking
  if (yaml_conf["networking"]) {
    auto networking = yaml_conf["networking"];
    if (networking["port"]) {
      port_ = networking["port"].as<u32>();
    }
    if (networking["neighborhood_size"]) {
      neighborhood_size_ = networking["neighborhood_size"].as<u32>();
    }
    if (networking["hostfile"]) {
      hostfile_path_ = networking["hostfile"].as<std::string>();
    }
  }

  // Segment names are hardcoded and expanded in ipc_manager.cc
  // No configuration needed here

  // Parse performance tuning configuration
  if (yaml_conf["performance"]) {
    auto perf = yaml_conf["performance"];
    if (perf["lane_map_policy"]) {
      std::string policy_str = perf["lane_map_policy"].as<std::string>();
      if (policy_str == "map_by_pid_tid") {
        lane_map_policy_ = LaneMapPolicy::kMapByPidTid;
      } else if (policy_str == "round_robin") {
        lane_map_policy_ = LaneMapPolicy::kRoundRobin;
      } else if (policy_str == "random") {
        lane_map_policy_ = LaneMapPolicy::kRandom;
      } else {
        HELOG(kWarning, "Unknown lane_map_policy '{}', using default (round_robin)", policy_str);
        lane_map_policy_ = LaneMapPolicy::kRoundRobin;
      }
    }
  }
}

} // namespace chi