
## Worker
Resolving a task should be updated to support distributed scheduling.

There are a few cases. First, if GetDynamic was used, then get the local container and call the Monitor function using the MonitorMode kGlobalSchedule. This will replace the domain query with something more concrete. Next, if the task does not resolve to kLocal addresses, then send the task to the local remote queue container for scheduling. If the task is local, then get the container to send this task to. Call the Monitor function with the kLocalSchedule MonitorMode to route the task to a specific lane. If the lane was initially empty, then the worker processing it likely will ignore it. 


# Remote Queue ChiMod Design

The Remote Queue ChiMod is a foundational component that enables distributed task execution across nodes in the Chimaera cluster.

## Core Functionality

**Inter-Node Communication**: Handles task distribution and result collection across the distributed system
**Task Serialization**: Manages efficient serialization/deserialization of task parameters and data
**Bulk Data Transfer**: Supports large binary data movement with optimized transfer mechanisms
**Archive Management**: Provides four distinct archive types for different serialization needs

## Task Serialization

Implement serializers that serialize different parts of the task. Tasks implement methods named SerializeIn and SerializeOut.
- **SerializeIn**: (De)serializes task entries labeled "IN" or "INOUT"
- **SerializeOut**: (De)serializes task parameters labeled "OUT" or "INOUT"

### Bulk Data Transfer Function

```cpp
bulk(hipc::Pointer p, size_t size, u32 flags);
```

**Transfer Flags**:
- **CHI_WRITE**: The data of pointer p should be copied to the remote location
- **CHI_EXPOSE**: The pointer p should be copied to the remote so the remote can write to it

### Archive Types

Four distinct archive types handle different serialization scenarios:
- **TaskOutputArchiveIN**: Serialize IN params of task using SerializeIn
- **TaskInputArchiveIN**: Deserialize IN params of task using SerializeIn
- **TaskOutputArchiveOUT**: Serialize OUT params of task using SerializeOut  
- **TaskInputArchiveOUT**: Deserialize OUT params of task using SerializeOut

# Container Processing

### Container Server
```cpp
namespace chi {

/**
 * Represents a custom operation to perform.
 * Tasks are independent of Hermes.
 * */
#ifdef CHIMAERA_RUNTIME
class ContainerRuntime {
public:
  PoolId pool_id_;           /**< The unique name of a pool */
  std::string pool_name_;    /**< The unique semantic name of a pool */
  ContainerId container_id_; /**< The logical id of a container */

  /** Create a lane group */
  void CreateQueue(QueueId queue_id, u32 num_lanes, chi::IntFlag flags);

  /** Get lane */
  Lane *GetLane(QueueId queue_id, LaneId lane_id);

  /** Get lane */
  Lane *GetLaneByHash(QueueId queue_id, u32 hash);

  /** Virtual destructor */
  HSHM_DLL virtual ~Module() = default;

  /** Run a method of the task */
  HSHM_DLL virtual void Run(u32 method, Task *task, RunContext &rctx) = 0;

  /** Monitor a method of the task */
  HSHM_DLL virtual void Monitor(MonitorModeId mode, u32 method, hipc::FullPtr<Task> task,
                                RunContext &rctx) = 0;

  /** Delete a task */
  HSHM_DLL virtual void Del(const hipc::MemContext &ctx, u32 method,
                            hipc::FullPtr<Task> task) = 0;

  /** Duplicate a task into a new task */
  HSHM_DLL virtual void NewCopy(u32 method, 
                                const hipc::FullPtr<Task> &orig_task,
                                hipc::FullPtr<Task> &dup_task, bool deep) = 0;

  /** Serialize a task inputs */
  HSHM_DLL virtual void SaveIn(u32 method, chi::TaskOutputArchiveIN &ar,
                               Task *task) = 0;

  /** Deserialize task inputs */
  HSHM_DLL virtual TaskPointer LoadIn(u32 method,
                                      chi::TaskInputArchiveIN &ar) = 0;

  /** Serialize task inputs */
  HSHM_DLL virtual void SerializeOut(u32 method, chi::TaskOutputArchiveOUT &ar,
                                Task *task) = 0;

  /** Deserialize task outputs */
  HSHM_DLL virtual void LoadOut(u32 method, chi::TaskInputArchiveOUT &ar,
                                Task *task) = 0;
};
#endif // CHIMAERA_RUNTIME
} // namespace chi
```