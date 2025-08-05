# Pool Manager
Create a PoolManager that stores the set of pools and containers within those pools located on this node. Remove that code from the worker. Create a singleton for the PoolManager. Remove the local container map from worker.

# Easy Fixes
Fix HSHM_GET_GLOBAL_CROSS_PTR_VAR second parameter. For example, 
``HSHM_GET_GLOBAL_CROSS_PTR_VAR(chi::IpcManager, chi::chiIpcManager)`` should be ``HSHM_GET_GLOBAL_CROSS_PTR_VAR(chi::IpcManager, chiIpcManager)``

Fix the logging statements. For example, `` HELOG(hshm::kError, "Failed to get container for task");`` should be ``HELOG(kError, "Failed to get container for task")``. 

Rename ``src/work_orchestrator/worker_impl.cc`` to ``src/work_orchestrator/worker.cc``.

In the worker, do not do GetOrCreate container when initially resolving the task. If the container does not exist, it should throw an error and then immediately complete the task.

The worker and work orchestrator code should be in separate header and source files.

# Routing to lane
``container->Monitor(MonitorMode::kLocalSchedule, task->method_, task, rctx);`` should put the lane suggestion inside of rctx. It should return this value immediately. 

The container should be loaded, from that container get the lane by id, and then place in the lane. Note that workers are separate threads, so lanes need to be thread safe. The best tool is chi::mpsc_queue for this since it is lockfree.

# Worker iteration
ExecuteInFiber could wait infinitely due to the last loop at the end. If we return from the fiber and the task is not complete, we would simply enqueue the task again in the same lane and then proceed executing more tasks.  