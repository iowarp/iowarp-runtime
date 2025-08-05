Don't have ipc_manager_base and ipc_manager.h. Instead, have a single IPC manager class that implements a ServerInit and ClientInit method. Also ensure that the code for connecting and creating shared memory with hshm is correct. An example is below:
```cpp
template <typename AllocT>
void PretestRank0() {
  std::string shm_url = "test_allocators2";
  AllocatorId alloc_id(1, 0);
  auto mem_mngr = HSHM_MEMORY_MANAGER;
  mem_mngr->UnregisterAllocator(alloc_id);
  mem_mngr->DestroyBackend(hipc::MemoryBackendId::GetRoot());
  mem_mngr->CreateBackend<PosixShmMmap>(hipc::MemoryBackendId::Get(0),
                                        hshm::Unit<size_t>::Megabytes(100),
                                        shm_url);
  mem_mngr->CreateAllocator<AllocT>(hipc::MemoryBackendId::Get(0), alloc_id,
                                    sizeof(sub::ipc::mpsc_ptr_queue<int>));
}

void PretestRankN() {
  std::string shm_url = "test_allocators2";
  AllocatorId alloc_id(1, 0);
  auto mem_mngr = HSHM_MEMORY_MANAGER;
  mem_mngr->AttachBackend(MemoryBackendType::kPosixShmMmap, shm_url);
}

void MainPretest() {
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  HILOG(kInfo, "PRETEST RANK 0 beginning {}", rank);
  if (rank == RANK0) {
    PretestRank0<HSHM_DEFAULT_ALLOC_T>();
  }
  MPI_Barrier(MPI_COMM_WORLD);
  if (rank != RANK0) {
    PretestRankN();
  }
  HILOG(kInfo, "PRETEST RANK 0 done {}", rank);
  MPI_Barrier(MPI_COMM_WORLD);
}

void MainPosttest() {}

```

Methods of the IPC manager that are runtime-only should be guarded with CHIMAERA_RUNTIME ifdefs.