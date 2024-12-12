//
// Created by llogan on 7/22/24.
//
#include "chimaera/api/chimaera_runtime.h"

namespace chi {

/** Create a container */
bool ModuleRegistry::CreateContainer(
    const char *lib_name, const char *pool_name, const PoolId &pool_id,
    Admin::CreateContainerBaseTask<Admin::CreateTaskParams> *task,
    const std::vector<SubDomainId> &containers) {
  ScopedMutex lock(lock_, 0);
  // Ensure pool_id is not NULL
  if (pool_id.IsNull()) {
    HELOG(kError, "The pool ID cannot be null");
    task->SetModuleComplete();
    return false;
  }
  //    HILOG(kInfo, "(node {}) Creating an instance of {} with name {}",
  //          CHI_CLIENT->node_id_, lib_name, pool_name);

  // Find the module to instantiate
  auto it = libs_.find(lib_name);
  if (it == libs_.end()) {
    HELOG(kError, "Could not find the task lib: {}", lib_name);
    task->SetModuleComplete();
    return false;
  }
  ModuleInfo &info = it->second;
  pools_[pool_id].module_ = &info;

  // Create partitioned state
  pools_[pool_id].lib_name_ = lib_name;
  std::unordered_map<ContainerId, Container *> &states =
      pools_[pool_id].containers_;
  for (const SubDomainId &container_id : containers) {
    // Don't repeat if state exists
    if (states.find(container_id.minor_) != states.end()) {
      continue;
    }

    // Allocate the state
    Container *exec = info.new_state_(&pool_id, pool_name);
    if (!exec) {
      HELOG(kError, "Could not create the pool: {}", pool_name);
      task->SetModuleComplete();
      return false;
    }

    // Add the state to the registry
    exec->id_ = pool_id;
    exec->name_ = pool_name;
    exec->container_id_ = container_id.minor_;
    pools_[pool_id].containers_[exec->container_id_] = exec;

    // Construct the state
    task->ctx_.id_ = pool_id;
    lock.Unlock();  // May spawn subtask that needs the lock
    exec->Run(TaskMethod::kCreate, task, task->rctx_);
    lock.Lock(0);
    exec->is_created_ = true;
    task->UnsetModuleComplete();
  }
  HILOG(kInfo,
        "(node {})  Created an instance of {} with pool name {} "
        "and pool ID {} ({} containers)",
        CHI_CLIENT->node_id_, lib_name, pool_name, pool_id, containers.size());
  return true;
}

}  // namespace chi
