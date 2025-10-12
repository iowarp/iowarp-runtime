@CLAUDE.md Implement the following methods in the runtime code for the admin chimod. Also update the archives in @include/chimaera/task_archives.h accordingly. Use ZeroMQ apis directly for this. Do not write stub implementations.

The admin should store an unordered_map of tasks. Let's do one map per worker to improve scalability. It should map TaskId to FullPtr<Task>. Tasks should not appear multiple times in the map. The task should be placed in the map during ClientSaveIn. ServerSaveOut, we should store a vector of unique task ids. ClientLoadOut should iterate over the unique task ids and deserialize the task return values. It should then remove the task from the map. ClientSaveIn should use the subtasks_ vector for storing replicas. Each replica should be given an id. We should rename minor_ in the TaskId to replica_id_. We should not increment replica_id_ when creating a TaskId, we will rely on unique from now on for unique ids. In addition, the node_id should be stored in the TaskId.

# ServerSaveOut

Similar to ClientSaveInArchive. This will serialize the outputs of the task. Key differences:
1. ServerSaveOutArchive should store a vector of TaskId, representing the saved unique tasks. 
2. It should call task->SerializeOut
3. The subtask should be deleted after being serialized.

# ClientLoadOut

Similar to ServerLoadIn, with the following exception:
1. It will deserialize existing tasks, not allocate new ones. It will find the existing tasks using the TaskId. It will then get the RunContext for the task. It will index subtasks vector in the RunContext to get the replica task. RunContext should store an atomic counter for counting the number of replicas that returned.
2. When all replicas complete, we aggregate the replicas into the original task.
3. The original task is then marked as completed if it is not periodic.

# ServerLoadInArchive

Server load task inputs. 
1. Receive the BuildMessage() and deserialize that into a TaskLoadInArchive using cereal::BinaryInputArchive. Both TaskLoadInArchive and TaskLoadOutArchive should have the same class variables, just different methods, so this should work.
2. Deserialize tasks one at a time using TaskLoadInArchive. ar(x, y, z) should just be the reverse from ClientSaveInArchive. ar.bulk() should call CHI_IPC->AllocateBuffer() to create new space. ar.bulk() should take as input a hipc::Pointer from the task. We then update the pointer.
3. Receive all remaining parts of the message and copy them into their respective DataTransfer locations. Use native ZeroMQ, not lightbeam.
4. Use ipc_manager to enqueue the tasks 


# ClientSaveInArchive

Client send task inputs. Corresponds to a TaskSaveInArchive
1. This function takes as input a ClientSendTaskInTask
2. Internally, this contains a subtask, which is the target for this function. 
3. We get the local container associated with the subtask using pool_id_.
4. We then build a TaskSaveInArchive, which will serialize task inputs.
5. We then call container->SaveIn(subtask, ar). This will serialize the subtask into the archive. The archive stores a vector of DataTransfer objects and a ceral::BinaryOutputArchive. ar(task) will call the task's SerializeIn method. Let's say task wants to serialize x, y, z, and data. ar(x, y, z) will serialize x, y, z into the binary output archive. ar.bulk(data) will add the data transfer to the vector.
6. Theoretically, multiple tasks could be saved in a single archive, but we aren't doing that for now
7. After we have serialized all tasks (for now just one) into the TaskSaveIn archive, we will need to build a message. TaskSaveInArchive should expose a function called BuildMessage(). This will serialize the TaskSaveInArchive itself, including the following: the number of tasks being serialized, the stringstream used for the BinaryOutputArchive, and the DataTransfer vector. The vector will not serialize the data that DataTransfer points to, it will just treat the pointers like integers. Use hipc::FullPtr(hipc::Pointer) constructor to create a FullPtr. Let's store a FullPtr in DataTransfer instead.
8. Send the messages to the resolved domains using ZeroMQ.
    1. Send the BuildMessage() return value.
    2. Iterate over DataTransfer and send the data objects it points to.
    3. Ensure the message is received as a single unit on the server. For ZeroMQ, this can be done with a flag ZMQ_SNDMORE. Make sure ZeroMQ is non-blocking as well when it connects to the remote server.

