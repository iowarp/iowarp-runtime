@CLAUDE.md Implement the following methods in the runtime code for the admin chimod. Also update the archives in @include/chimaera/task_archives.h accordingly. Use ZeroMQ apis directly for this. Do not write stub implementations.

The admin should store an unordered_map of tasks. Tasks should not appear multiple times in the map. We should replace TaskNode type with TaskId. It should have the same parameters as TaskNode and should behave similar, with one exception: it should also have a parameter called "unique", which increments the same counter as major, which is called during construction or increment,
but not during copy.

# ServerSaveOutArchive

Similar to ClientSaveInArchive, with one major exception: it will call SerializeOut instead of SerializeIn. This will serialize the outputs of the task.

# ClientLoadOutArchive

Similar to ServerLoadIn, with two major exceptions:
1. It will deserialize the ClientLoadOutArchive type similar to ServerLoadIn does for ServerLoadInArchive.
1. The deserialized 
1. It must locate an existing task.
2. It will call SerializeOut

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

