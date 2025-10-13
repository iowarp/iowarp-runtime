@CLAUDE.md Implement the following methods in the runtime code for the admin chimod. Also update the archives in @include/chimaera/task_archives.h accordingly. Use ZeroMQ apis directly for this. Do not write stub implementations. 

I want to replace ClientSendTaskIn, ClientRecvTaskOut, ServerRecvTaskIn, and ServerSendTaskOut with just two functions: Recv and Send. Update the chimod to have only these two functions. Rename the tasks and method ids accordingly. Check @docs/MODULE_DEVELOPMENT_GUIDE.md for details on how to modify chimods. In addition, we will be replacing the corresponding archives. for these functions. Ther will be just two archives: SaveTaskArchive and LoadTaskArchive.

# Send
In the admin_runtime.cc, Send is implemented as follows:

Send either task inputs or outputs. The SendTask should have the following:
1. A boolean indicating whether we are sending inputs or outputs (srl_mode)
2. A FullPtr to a subtask to serialize and send over the network (subtask)
3. A vector of the resolved pool queries

## SerializeIn mode
Sending task inputs.
1. We get the local container associated with the subtask using pool_id_. 
2. Add the subtask (in this case the origin task) to an unordered_map (send_map) stored in the Admin class, which maps TaskId -> FullPtr<Task>. This will allow the Recv function to locate the task later. 
3. We then send messages to the resolved pool queries using ZeroMQ in a loop as follows:
    

## SerializeOut mode
Send task outputs.
1. We get the local container associated with the subtask using pool_id_. 
2. Remove the task from the recv_map.
3. We then send messages to the return node stored in the task using the SendTask loop. We create a physical pool query

## SendTask loop (common to both modes)
Takes as input vector of PoolQuery.
    1. Get the address of the node we are sending data to. If a range query, use the start of the range. It is a container id, so convert to address using pool manager. If direct, convert to address using pool manager. If physical, convert to address using CHI_IPC.
    2. Construct a SaveTaskArchive that takes as input the boolean srl_mode and the container. This archive is a wrapper around cereal::BinaryOutputArchive.
    3. ONLY FOR SerializeIn: Make a copy of the task using container->NewCopy. Update the copy's pool query to be the current query. Add the copy to the RunContext of the original task under subtasks vector. Update the minor_ of the copy's TaskId to be its index in the subtasks vector. Update the ret_node_ field of the task to be the ID of this node according to CHI_IPC. ret_node_ should be stored in the PoolQuery as a u32.
    4. Call ``ar << task`` to serialize the task.
    5. Serialize the SaveTaskArchive itself. SaveTaskArchive should expose a function called BuildMessage() to do this.
    6. Iterate over DataTransfer and send the data objects it points to.
    7. Ensure the message is received as a single unit on the server. For ZeroMQ, this can be done with a flag ZMQ_SNDMORE. Make sure ZeroMQ is non-blocking as well when it connects to the remote server.
    8. ONLY FOR SerializeOut: Delete the task. We are returning its outputs since the task is completed on this node.

## SaveTaskArchive
constructor:
1. srl_mode

Stores the following:
1. a vector of DataTransfer objects
2. a cereal::BinaryOutputArchive
3. a vector of <TaskId, PoolId, MethodId> (task info)

``ar << task`` should do the following:
1. Append the task id, pool id, and method id to the vector of task info
2. Call either task->SerializeIn or task->SerializeOut depending on the srl_mode

SerializeIn and SerializeOut may call the following internally:
1. Let's say task wants to serialize x, y, z. ar(x, y, z) will serialize x, y, z into the binary output archive. x, y, z are checked if they inherit from chi::Task for this using compile-time checks. If they are tasks, then SerializeIn or SerializeOut would be called.
2. Let's say task wants to serialize data. ar.bulk(data) will add the data transfer to the vector.

# Recv

This will either execute a task or complete a task. 
1. Receives the BuildMessage() and deserializes it into a LoadTaskArchive. LoadTaskArchive should have the same structure and class variables as SaveTaskArchive. Its only difference should be in the implementation of its methods.
2. Check if srl_mode in the LoadTaskArchive is for SerializeIn or SerializeOut. 

## SerializeIn srl_mode

This is when the server receives task inputs, meaning we are just beginning execution.
Deserialize tasks one at a time by iterating over the task info in a for loop. Each loop iteration as follows:
1. Get the container associated with PoolId
2. Create (but do not allocate) a task pointer ``Task *task``
3. Do ``ar >> task``, which will allocate and deserialize the task.
4. Receive all remaining parts of the message and copy them into their respective DataTransfer locations.
5. Add the task to an unordered_map TaskId -> FullPtr<Task> (recv_map). 
6. Use ipc_manager to enqueue the tasks 

## SerializeOut srl_mode 

This is when the server receives task outputs, meaning we are ending execution. 
Deserialize tasks one at a time by iterating over the task info in a for loop. Each loop iteration as follows:
1. Locate the origin task from the send_map.
2. Locate the replica in the origin's run_ctx
3. Do ``ar >> replica``
4. Receive all remaining parts of the message and copy them into their respective DataTransfer locations.
5. Increment atomic counter tracking the set of replicas that have completed in the run_ctx of the origin task.
6. If the count is equal to the number of replicas, remove origin from the map, clear subtasks, reset counter, and then:
  1. If not periodic, mark as completed.
  2. Else, do nothing.

## LoadTaskArchive
Same structure as SaveTaskArchive and mostly same class variables. cereal::BinaryInputArchive instead.

ar(x, y, z) should just be the reverse from ClientSaveInArchive. 

### SerializeIn srl_mode
ar.bulk() should call CHI_IPC->AllocateBuffer() to create new space. ar.bulk() should take as input a hipc::Pointer from the task. We then update the pointer.

### SerializeOut srl_mode
ar.bulk should do nothing, since the task already exists.
