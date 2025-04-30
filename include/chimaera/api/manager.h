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
  QueueManagerShm queue_manager_;
  hipc::atomic<hshm::min_u64> unique_;
  u64 num_nodes_;
  NodeId node_id_;
};

#define MAX_GPU 4

/** The configuration used inherited by runtime + client */
class ConfigurationManager {
 public:
  ChiShm *header_;
  ClientConfig *client_config_;
  ServerConfig *server_config_;
  static inline const hipc::AllocatorId main_alloc_id_ =
      hipc::AllocatorId(1, 0);
  static inline const hipc::AllocatorId data_alloc_id_ =
      hipc::AllocatorId(2, 0);
  static inline const hipc::AllocatorId rdata_alloc_id_ =
      hipc::AllocatorId(3, 0);
  CHI_MAIN_ALLOC_T *main_alloc_;
  CHI_DATA_ALLOC_T *data_alloc_;
  CHI_RDATA_ALLOC_T *rdata_alloc_;
  CHI_SHM_GPU_ALLOC_T *gpu_alloc_[MAX_GPU];
  int ngpu_ = 0;
  bool is_being_initialized_;
  bool is_initialized_;
  bool is_terminated_;
  bool is_transparent_;
  hshm::Mutex lock_;
  hshm::ThreadType thread_type_;

  /** Refresh the number of GPUs */
  void RefreshNumGpus() { ngpu_ = hshm::GpuApi::GetDeviceCount(); }

  /** Get GPU mem backend id */
  HSHM_INLINE_CROSS_FUN static hipc::MemoryBackendId GetGpuCpuBackendId(
      int gpu_id) {
    return hipc::MemoryBackendId::Get(3 + gpu_id * 2);
  }

  /** Get GPU mem backend id */
  HSHM_INLINE_CROSS_FUN static hipc::MemoryBackendId GetGpuDataBackendId(
      int gpu_id) {
    return hipc::MemoryBackendId::Get(3 + gpu_id * 2 + 1);
  }

  /** Get GPU allocator id */
  HSHM_INLINE_CROSS_FUN static hipc::AllocatorId GetGpuCpuAllocId(int gpu_id) {
    return hipc::AllocatorId(3 + gpu_id * 2, 0);
  }

  /** Get GPU allocator id */
  HSHM_INLINE_CROSS_FUN static hipc::AllocatorId GetGpuDataAllocId(int gpu_id) {
    return hipc::AllocatorId(3 + gpu_id * 2 + 1, 0);
  }

  /** Get GPU mem backend name */
  std::string GetGpuCpuAllocName(int gpu_id) {
    return server_config_->queue_manager_.base_gpu_cpu_name_ +
           std::to_string(gpu_id);
  }

  /** Get GPU data laloc name */
  std::string GetGpuDataAllocName(int gpu_id) {
    return server_config_->queue_manager_.base_gpu_data_name_ +
           std::to_string(gpu_id);
  }

  /** Is the pointer a GPU data pointer? */
  bool IsGpuDataPointer(const hipc::Pointer &ptr) {
    if (ptr.IsNull()) return false;
    for (int i = 0; i < ngpu_; ++i) {
      if (ptr.alloc_id_ == GetGpuDataAllocId(i)) {
        return true;
      }
    }
    return false;
  }

  /** Get GPU allocator */
  HSHM_INLINE_CROSS_FUN CHI_SHM_GPU_ALLOC_T *GetGpuAlloc(int gpu_id) {
    return gpu_alloc_[gpu_id];
  }

  /** Get GPU allocator */
  HSHM_INLINE_CROSS_FUN CHI_DATA_GPU_ALLOC_T *GetGpuDataAlloc(int gpu_id) {
    return HSHM_MEMORY_MANAGER->GetAllocator<CHI_DATA_GPU_ALLOC_T>(
        GetGpuDataAllocId(gpu_id));
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
  HSHM_INLINE_CROSS_FUN bool IsBeingInitialized() {
    return is_being_initialized_;
  }

  /** Whether or not CHI is initialized */
  HSHM_INLINE_CROSS_FUN bool IsInitialized() { return is_initialized_; }

  /** Whether or not CHI is finalized */
  HSHM_INLINE_CROSS_FUN bool IsTerminated() { return is_terminated_; }

  /** Load the server-side configuration */
  void LoadServerConfig(std::string config_path) {
    server_config_ = new ServerConfig();
    if (config_path.empty()) {
      config_path = HSHM_SYSTEM_INFO->Getenv(Constants::kServerConfEnv);
    }
    HILOG(kInfo, "Loading server configuration: {}", config_path);
    server_config_->LoadFromFile(config_path);
  }

  /** Load the client-side configuration */
  void LoadClientConfig(std::string config_path) {
    client_config_ = new ClientConfig();
    if (config_path.empty()) {
      config_path = HSHM_SYSTEM_INFO->Getenv(Constants::kClientConfEnv);
    }
    client_config_->LoadFromFile(config_path);
  }

  /** Get number of nodes */
  HSHM_INLINE int GetNumNodes() {
    return server_config_->rpc_.host_names_.size();
  }
};

}  // namespace chi

#endif  // CHI_INCLUDE_CHI_MANAGER_MANAGER_H_
