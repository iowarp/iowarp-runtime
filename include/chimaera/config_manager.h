#ifndef CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_CONFIG_MANAGER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_CONFIG_MANAGER_H_

#include <string>
#include "chimaera/types.h"

namespace chi {

/**
 * Configuration manager singleton
 * 
 * Inherits from hshm BaseConfig and manages YAML configuration parsing.
 * Reads configuration from CHI_SERVER_CONF environment variable.
 * Uses HSHM global cross pointer variable singleton pattern.
 */
class ConfigManager : public hshm::BaseConfig {
 public:
  /**
   * Initialize configuration manager
   * @return true if initialization successful, false otherwise
   */
  bool Init();

  /**
   * Load configuration from YAML file
   * @param config_path Path to YAML configuration file
   * @return true if loading successful, false otherwise
   */
  bool LoadYaml(const std::string& config_path);

  /**
   * Get server configuration file path from environment
   * @return Configuration file path or empty string if not set
   */
  std::string GetServerConfigPath() const;

  /**
   * Get number of worker threads for given type
   * @param thread_type Type of worker thread
   * @return Number of threads to spawn
   */
  u32 GetWorkerThreadCount(ThreadType thread_type) const;

  /**
   * Get memory segment size
   * @param segment Memory segment identifier
   * @return Size in bytes
   */
  size_t GetMemorySegmentSize(MemorySegment segment) const;

  /**
   * Get ZeroMQ server port
   * @return Port number for ZeroMQ server
   */
  u32 GetZmqPort() const;

  /**
   * Check if configuration is valid
   * @return true if configuration is valid and loaded
   */
  bool IsValid() const;

 private:
  /**
   * Set default configuration values (implements hshm::BaseConfig)
   */
  void LoadDefault() override;
  
  /**
   * Parse YAML configuration (implements hshm::BaseConfig)
   */
  void ParseYAML(YAML::Node &yaml_conf) override;

  bool is_initialized_ = false;
  std::string config_file_path_;
  
  // Configuration parameters
  u32 low_latency_workers_ = 2;
  u32 high_latency_workers_ = 4;
  u32 reinforcement_workers_ = 1;
  u32 process_reaper_workers_ = 1;
  
  size_t main_segment_size_ = hshm::Unit<size_t>::Gigabytes(1);
  size_t client_data_segment_size_ = hshm::Unit<size_t>::Megabytes(256);
  size_t runtime_data_segment_size_ = hshm::Unit<size_t>::Megabytes(256);
  
  u32 zmq_port_ = 8080;
};

}  // namespace chi

// Macro for accessing the Configuration manager singleton using HSHM singleton
#define CHI_CONFIG hshm::Singleton<ConfigManager>::GetInstance()

#endif  // CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_CONFIG_MANAGER_H_