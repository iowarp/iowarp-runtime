/**
 * Configuration manager implementation
 */

#include "chimaera/config_manager.h"
#include <cstdlib>
#include <iostream>

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

  // Load YAML configuration if path is provided
  if (!config_file_path_.empty()) {
    if (!LoadYaml(config_file_path_)) {
      std::cerr << "Warning: Failed to load configuration from " 
                << config_file_path_ << ", using defaults" << std::endl;
    }
  }

  is_initialized_ = true;
  return true;
}

bool ConfigManager::ServerInit() {
  // Configuration is needed by both client and server, so same implementation
  return ClientInit();
}

bool ConfigManager::LoadYaml(const std::string& config_path) {
  try {
    // Use HSHM BaseConfig methods
    LoadFromFile(config_path, true);
    return true;
  } catch (const std::exception& e) {
    return false;
  }
}

std::string ConfigManager::GetServerConfigPath() const {
  const char* env_path = std::getenv("CHI_SERVER_CONF");
  return env_path ? std::string(env_path) : std::string();
}

u32 ConfigManager::GetWorkerThreadCount(ThreadType thread_type) const {
  switch (thread_type) {
    case kLowLatencyWorker:
      return low_latency_workers_;
    case kHighLatencyWorker:
      return high_latency_workers_;
    case kReinforcementWorker:
      return reinforcement_workers_;
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

u32 ConfigManager::GetZmqPort() const {
  return zmq_port_;
}

u32 ConfigManager::GetTaskQueueLanes() const {
  return task_queue_lanes_;
}

std::string ConfigManager::GetSharedMemorySegmentName(MemorySegment segment) const {
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

bool ConfigManager::IsValid() const {
  return is_initialized_;
}

void ConfigManager::LoadDefault() {
  // Set default configuration values
  low_latency_workers_ = 4;
  high_latency_workers_ = 2;
  reinforcement_workers_ = 1;
  process_reaper_workers_ = 1;
  
  main_segment_size_ = 1024 * 1024 * 1024; // 1GB
  client_data_segment_size_ = 512 * 1024 * 1024; // 512MB
  runtime_data_segment_size_ = 512 * 1024 * 1024; // 512MB
  
  zmq_port_ = 5555;
  task_queue_lanes_ = 4;
  
  // Set default shared memory segment names with environment variables
  main_segment_name_ = "chi_main_segment_${USER}";
  client_data_segment_name_ = "chi_client_data_segment_${USER}";
  runtime_data_segment_name_ = "chi_runtime_data_segment_${USER}";
}

void ConfigManager::ParseYAML(YAML::Node &yaml_conf) {
  // Parse worker thread counts
  if (yaml_conf["workers"]) {
    auto workers = yaml_conf["workers"];
    if (workers["low_latency"]) {
      low_latency_workers_ = workers["low_latency"].as<u32>();
    }
    if (workers["high_latency"]) {
      high_latency_workers_ = workers["high_latency"].as<u32>();
    }
    if (workers["reinforcement"]) {
      reinforcement_workers_ = workers["reinforcement"].as<u32>();
    }
    if (workers["process_reaper"]) {
      process_reaper_workers_ = workers["process_reaper"].as<u32>();
    }
  }
  
  // Parse memory segments
  if (yaml_conf["memory"]) {
    auto memory = yaml_conf["memory"];
    if (memory["main_segment_size"]) {
      main_segment_size_ = hshm::ConfigParse::ParseSize(
        memory["main_segment_size"].as<std::string>()
      );
    }
    if (memory["client_data_segment_size"]) {
      client_data_segment_size_ = hshm::ConfigParse::ParseSize(
        memory["client_data_segment_size"].as<std::string>()
      );
    }
    if (memory["runtime_data_segment_size"]) {
      runtime_data_segment_size_ = hshm::ConfigParse::ParseSize(
        memory["runtime_data_segment_size"].as<std::string>()
      );
    }
  }
  
  // Parse networking
  if (yaml_conf["networking"]) {
    auto networking = yaml_conf["networking"];
    if (networking["zmq_port"]) {
      zmq_port_ = networking["zmq_port"].as<u32>();
    }
  }
  
  // Parse task queue configuration
  if (yaml_conf["task_queue"]) {
    auto task_queue = yaml_conf["task_queue"];
    if (task_queue["lanes"]) {
      task_queue_lanes_ = task_queue["lanes"].as<u32>();
    }
  }
  
  // Parse shared memory configuration
  if (yaml_conf["shared_memory"]) {
    auto shm = yaml_conf["shared_memory"];
    if (shm["main_segment_name"]) {
      main_segment_name_ = shm["main_segment_name"].as<std::string>();
    }
    if (shm["client_data_segment_name"]) {
      client_data_segment_name_ = shm["client_data_segment_name"].as<std::string>();
    }
    if (shm["runtime_data_segment_name"]) {
      runtime_data_segment_name_ = shm["runtime_data_segment_name"].as<std::string>();
    }
  }
}

}  // namespace chi