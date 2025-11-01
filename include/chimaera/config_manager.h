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
   * Initialize configuration manager (generic wrapper)
   * Loads configuration from environment variables and files
   * @return true if initialization successful, false otherwise
   */
  bool Init() { return ClientInit(); }

  /**
   * Initialize configuration manager (client mode)
   * Loads configuration from environment variables and files
   * @return true if initialization successful, false otherwise
   */
  bool ClientInit();

  /**
   * Initialize configuration manager (server/runtime mode)
   * Same as ClientInit since config is needed by both
   * @return true if initialization successful, false otherwise
   */
  bool ServerInit();

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
   * Get networking port
   * @return Port number for networking
   */
  u32 GetPort() const;

  /**
   * Get neighborhood size for range query splitting
   * @return Maximum number of queries when splitting range queries
   */
  u32 GetNeighborhoodSize() const;

  /**
   * Get shared memory segment names
   * @param segment Memory segment identifier`
   * @return Expanded segment name with environment variables resolved
   */
  std::string GetSharedMemorySegmentName(MemorySegment segment) const;

  /**
   * Get hostfile path for distributed scheduling
   * @return Path to hostfile with list of available nodes
   */
  std::string GetHostfilePath() const;

  /**
   * Check if configuration is valid
   * @return true if configuration is valid and loaded
   */
  bool IsValid() const;

  /**
   * Get lane mapping policy for task distribution
   * @return Lane mapping policy
   */
  LaneMapPolicy GetLaneMapPolicy() const;

 private:
  /**
   * Set default configuration values (implements hshm::BaseConfig)
   */
  void LoadDefault() override;

  /**
   * Parse YAML configuration (implements hshm::BaseConfig)
   */
  void ParseYAML(YAML::Node& yaml_conf) override;

  bool is_initialized_ = false;
  std::string config_file_path_;

  // Configuration parameters
  u32 sched_workers_ = 8;
  u32 process_reaper_workers_ = 1;

  size_t main_segment_size_ = hshm::Unit<size_t>::Gigabytes(1);
  size_t client_data_segment_size_ = hshm::Unit<size_t>::Megabytes(256);
  size_t runtime_data_segment_size_ = hshm::Unit<size_t>::Megabytes(256);

  u32 port_ = 9128;
  u32 neighborhood_size_ = 32;

  // Shared memory segment names with environment variable support
  std::string main_segment_name_ = "chi_main_segment_${USER}";
  std::string client_data_segment_name_ = "chi_client_data_segment_${USER}";
  std::string runtime_data_segment_name_ = "chi_runtime_data_segment_${USER}";

  // Networking configuration
  std::string hostfile_path_ = "";

  // Task distribution policy
  LaneMapPolicy lane_map_policy_ = LaneMapPolicy::kRoundRobin;
};

}  // namespace chi

// Global pointer variable declaration for Configuration manager singleton
HSHM_DEFINE_GLOBAL_PTR_VAR_H(chi::ConfigManager, g_config_manager);

// Macro for accessing the Configuration manager singleton using global pointer variable
#define CHI_CONFIG_MANAGER HSHM_GET_GLOBAL_PTR_VAR(::chi::ConfigManager, g_config_manager)

#endif  // CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_CONFIG_MANAGER_H_