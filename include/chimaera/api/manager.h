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

#ifndef CHI_INCLUDE_CHI_MANAGER_MANAGER_H_
#define CHI_INCLUDE_CHI_MANAGER_MANAGER_H_

#include "chimaera/chimaera_constants.h"
#include "chimaera/chimaera_types.h"
#include "chimaera/config/config_client.h"
#include "chimaera/config/config_server.h"
#include "chimaera/queue_manager/queue_manager.h"

namespace chi {

/** Shared-memory header for CHI */
struct ChiShm {
  NodeId node_id_;
  QueueManagerShm queue_manager_;
  hipc::atomic<u64> unique_;
  u64 num_nodes_;
};

#define MAX_GPU 16

/** The configuration used inherited by runtime + client */
class ConfigurationManager {
 public:
  ChiShm *header_;
  ClientConfig client_config_;
  ServerConfig server_config_;
  static inline const hipc::alloc_id_t main_alloc_id_ = hipc::alloc_id_t(0, 1);
  static inline const hipc::alloc_id_t data_alloc_id_ = hipc::alloc_id_t(1, 1);
  static inline const hipc::alloc_id_t rdata_alloc_id_ = hipc::alloc_id_t(2, 1);
  CHI_ALLOC_T *main_alloc_;
  CHI_ALLOC_T *data_alloc_;
  CHI_ALLOC_T *rdata_alloc_;
  CHI_ALLOC_T *gpu_alloc_[MAX_GPU];
  bool is_being_initialized_;
  bool is_initialized_;
  bool is_terminated_;
  bool is_transparent_;
  hshm::Mutex lock_;
  hshm::ThreadType thread_type_;

  /** Get GPU mem backend id */
  HSHM_INLINE_CROSS_FUN static hipc::MemoryBackendId GetGpuMemBackendId(
      int gpu_id) {
    return hipc::MemoryBackendId(3 + gpu_id);
  }

  /** Get GPU allocator id */
  HSHM_INLINE_CROSS_FUN static hipc::AllocatorId GetGpuAllocId(int gpu_id) {
    return hipc::AllocatorId(3 + gpu_id, 1);
  }

  /** Get GPU allocator */
  HSHM_INLINE_CROSS_FUN CHI_ALLOC_T *GetGpuAlloc(int gpu_id) {
    return gpu_alloc_[gpu_id];
  }

  /** Default constructor */
  HSHM_INLINE_CROSS_FUN ConfigurationManager()
      : is_being_initialized_(false),
        is_initialized_(false),
        is_terminated_(false),
        is_transparent_(false) {}

  /** Destructor */
  HSHM_INLINE_CROSS_FUN ~ConfigurationManager() {}

  /** Whether or not CHI is currently being initialized */
  bool IsBeingInitialized() { return is_being_initialized_; }

  /** Whether or not CHI is initialized */
  bool IsInitialized() { return is_initialized_; }

  /** Whether or not CHI is finalized */
  bool IsTerminated() { return is_terminated_; }

  /** Load the server-side configuration */
  void LoadServerConfig(std::string config_path) {
    if (config_path.empty()) {
      config_path = HERMES_SYSTEM_INFO->Getenv(Constants::kServerConfEnv);
    }
    HILOG(kInfo, "Loading server configuration: {}", config_path);
    server_config_.LoadFromFile(config_path);
  }

  /** Load the client-side configuration */
  void LoadClientConfig(std::string config_path) {
    if (config_path.empty()) {
      config_path = HERMES_SYSTEM_INFO->Getenv(Constants::kClientConfEnv);
    }
    client_config_.LoadFromFile(config_path);
  }

  /** Get number of nodes */
  HSHM_INLINE int GetNumNodes() {
    return server_config_.rpc_.host_names_.size();
  }
};

}  // namespace chi

#endif  // CHI_INCLUDE_CHI_MANAGER_MANAGER_H_
