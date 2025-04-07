#include "chimaera/api/chimaera_client.h"

#include "chimaera_admin/chimaera_admin_client.h"

namespace chi {

/** Initialize the client (GPU) */
#if defined(CHIMAERA_ENABLE_ROCM) || defined(CHIMAERA_ENABLE_CUDA)
HSHM_GPU_KERNEL static void CreateClientKernel(hipc::AllocatorId alloc_id) {
  auto *p = HSHM_MEMORY_MANAGER->GetAllocator<CHI_ALLOC_T>(alloc_id);
  CHI_CLIENT->CreateOnGpu(alloc_id);
}

HSHM_GPU_FUN
void Client::CreateOnGpu(hipc::AllocatorId alloc_id) {
  main_alloc_ = HSHM_MEMORY_MANAGER->GetAllocator<CHI_ALLOC_T>(alloc_id);
  data_alloc_ = nullptr;
  rdata_alloc_ = nullptr;
  HSHM_MEMORY_MANAGER->SetDefaultAllocator(main_alloc_);
  header_ = main_alloc_->GetCustomHeader<ChiShm>();
  unique_ = &header_->unique_;
  node_id_ = header_->node_id_;
  CHI_QM->ClientInit(main_alloc_, header_->queue_manager_, header_->node_id_);
  is_initialized_ = true;
  is_being_initialized_ = false;
}
#endif

/** Initialize the client */
Client *Client::Create(const char *server_config_path,
                       const char *client_config_path, bool server) {
  hshm::ScopedMutex lock(lock_, 1);
  if (is_initialized_) {
    return this;
  }
  is_being_initialized_ = true;
  ClientInit(server_config_path, client_config_path, server);
  is_initialized_ = true;
  is_being_initialized_ = false;
  return this;
}

static inline std::string get_process_filename() {
  char result[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
  return std::string(result, (count > 0) ? count : 0);
}

static inline void ExceptionTerminator() {
  try {
    std::exception_ptr currentException = std::current_exception();
    if (currentException) {
      std::rethrow_exception(currentException);
    }
  } catch (const std::exception &e) {
    std::cerr << "Uncaught exception: " << e.what() << std::endl;
  } catch (hshm::Error &e) {
    std::string exe = get_process_filename();
    HELOG(kError, "(node {}, exe {}) {}: \n {}", CHI_CLIENT->node_id_, exe,
          e.what(), boost::stacktrace::stacktrace());
  } catch (...) {
    std::cerr << "Uncaught exception of unknown type." << std::endl;
  }
  std::abort();  // Terminate after printing the error.
}

/** Initialize client */
void Client::ClientInit(const char *server_config_path,
                        const char *client_config_path, bool server) {
  std::set_terminate(ExceptionTerminator);
  LoadServerConfig(server_config_path);
  LoadClientConfig(client_config_path);
  LoadSharedMemory(server);
  CHI_QM->ClientInit(main_alloc_, header_->queue_manager_, header_->node_id_);
  CreateClientOnHostForGpu();
}

/** Connect to a Daemon's shared memory */
void Client::LoadSharedMemory(bool server) {
  // Load shared-memory allocator
  config::QueueManagerInfo &qm = server_config_->queue_manager_;
  auto mem_mngr = HSHM_MEMORY_MANAGER;
  if (!server) {
    mem_mngr->AttachBackend(hipc::MemoryBackendType::kPosixShmMmap,
                            qm.shm_name_);
    mem_mngr->AttachBackend(hipc::MemoryBackendType::kPosixShmMmap,
                            qm.data_shm_name_);
    mem_mngr->AttachBackend(hipc::MemoryBackendType::kPosixShmMmap,
                            qm.rdata_shm_name_);
  }
  main_alloc_ = mem_mngr->GetAllocator<CHI_ALLOC_T>(main_alloc_id_);
  data_alloc_ = mem_mngr->GetAllocator<CHI_ALLOC_T>(data_alloc_id_);
  rdata_alloc_ = mem_mngr->GetAllocator<CHI_ALLOC_T>(rdata_alloc_id_);
  mem_mngr->SetDefaultAllocator(main_alloc_);
  header_ = main_alloc_->GetCustomHeader<ChiShm>();
  unique_ = &header_->unique_;
  node_id_ = header_->node_id_;
  RefreshNumGpus();

  // Create per-gpu allocator
  if (!server) {
#ifdef CHIMAERA_ENABLE_ROCM
    LoadSharedMemoryGpu("rocm_shm_", hipc::MemoryBackendType::kRocmShmMmap,
                        hipc::MemoryBackendType::kRocmMalloc);
#endif
#ifdef CHIMAERA_ENABLE_CUDA
    LoadSharedMemoryGpu("cuda_shm_", hipc::MemoryBackendType::kCudaShmMmap,
                        hipc::MemoryBackendType::kCudaMalloc);
#endif
  }
}

/** Load the shared memory for GPUs */
void Client::LoadSharedMemoryGpu(const std::string &prefix,
                                 hipc::MemoryBackendType pinned,
                                 hipc::MemoryBackendType dev) {
  for (int gpu_id = 0; gpu_id < ngpu_; ++gpu_id) {
    hipc::MemoryBackendId backend_id = GetGpuMemBackendId(gpu_id);
    hipc::AllocatorId alloc_id = GetGpuAllocId(gpu_id);
    // TODO(llogan): Make parameter for gpu_shm_name_ and gpu_shm_size_
    hipc::chararr name = prefix + std::to_string(gpu_id);
    HSHM_MEMORY_MANAGER->AttachBackend(pinned, name);
    gpu_alloc_[gpu_id] = HSHM_MEMORY_MANAGER->GetAllocator<CHI_SHM_GPU_ALLOC_T>(
        GetGpuAllocId(gpu_id));
  }

  for (int gpu_id = 0; gpu_id < ngpu_; ++gpu_id) {
    hipc::MemoryBackendId backend_id = GetGpuDataBackendId(gpu_id);
    hipc::AllocatorId alloc_id = GetGpuDataAllocId(gpu_id);
    // TODO(llogan): Make parameter for gpu_shm_name_ and gpu_shm_size_
    hipc::chararr name = prefix + std::to_string(gpu_id);
    HSHM_MEMORY_MANAGER->AttachBackend(dev, name);
    gpu_data_alloc_[gpu_id] =
        HSHM_MEMORY_MANAGER->GetAllocator<CHI_DATA_GPU_ALLOC_T>(
            GetGpuDataAllocId(gpu_id));
  }
}

/** Creates the CHI_CLIENT on the GPU */
void Client::CreateClientOnHostForGpu() {
  // Get the allocators for the GPUs
  for (int gpu_id = 0; gpu_id < ngpu_; ++gpu_id) {
#if defined(CHIMAERA_ENABLE_ROCM)
    HIP_ERROR_CHECK(hipSetDevice(gpu_id));
    CreateClientKernel<<<1, 1>>>(GetGpuAllocId(gpu_id));
    HIP_ERROR_CHECK(hipDeviceSynchronize());
#elif defined(CHIMAERA_ENABLE_CUDA)
    cudaSetDevice(gpu_id);
    CreateClientKernel<<<1, 1>>>(GetGpuAllocId(gpu_id));
    cudaDeviceSynchronize();
#endif
  }
}

HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_CC(Client, chiClient);
HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_CC(QueueManager, chiQueueManager);
HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_CC(chi::Admin::Client,
                                    chi::Admin::chiAdminClient);

}  // namespace chi