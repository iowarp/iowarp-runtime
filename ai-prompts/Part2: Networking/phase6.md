@CLAUDE.md Implement the following methods in the runtime code for the admin chimod. Also update the archives in @include/chimaera/task_archives.h accordingly. Also read @docs/hshm/hshm-context.md to see how to use lightbeam.

# ClientSaveInArchive

Client send task inputs. Corresponds to a TaskSaveInArchive
1. This function takes as input a ClientSendTaskInTask
2. Internally, this contains a subtask, which is the target for this function. 
3. We get the local container associated with the subtask using pool_id_.
4. We then build a TaskSaveInArchive, which will serialize task inputs.
5. We then call container->SaveIn(subtask, ar). This will serialize the subtask into the archive. The archive stores a vector of DataTransfer objects and a ceral::BinaryOutputArchive. ar(task) will call the task's SerializeIn method. Let's say task wants to serialize x, y, z, and data. ar(x, y, z) will serialize x, y, z into the binary output archive. ar.bulk(data) will add the data transfer to the vector.
6. Theoretically, multiple tasks could be saved in a single archive, but we aren't doing that for now
7. After we have serialized all tasks (for now just one) into the TaskSaveIn archive, we will need to build a message. TaskSaveInArchive should expose a function called BuildMessage(). This will serialize the TaskSaveInArchive itself, including the following: the number of tasks being serialized, the stringstream used for the BinaryOutputArchive, and the DataTransfer vector. The vector will not serialize the data that DataTransfer points to, it will just treat the pointers like integers.
8. Send the messages to the resolved domains using ZeroMQ.
    1. Send the BuildMessage() return value.
    2. Iterate over DataTransfer and send the data objects it points to.
    3. Ensure the message is received as a single unit on the server. For ZeroMQ, this can be done with a flag ZMQ_SNDMORE. Make sure ZeroMQ is non-blocking as well when it connects to the remote server.

# ServerLoadInArchive

Server load task inputs. 
1. Receive the BuildMessage() and deserialize that into a TaskLoadInArchive using cereal::BinaryInputArchive. Both TaskLoadInArchive and TaskLoadOutArchive should have the same class variables, just different methods, so this should work.
2. Deserialize tasks one at a time using TaskLoadInArchive. ar(x, y, z) should just be the reverse from ClientSaveInArchive. ar.bulk() should call CHI_IPC->AllocateBuffer() to get the space, but do nothing else
3. Receive all remaining parts of the message and copy them into their respective DataTransfer locations.
4. 
