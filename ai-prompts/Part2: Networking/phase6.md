@CLAUDE.md Implement the following methods in the runtime code for the admin chimod.

# ClientSendTaskIn

1. Get the current lane.
2. Pop each task on the current lane.
3. Build an unordered map of node_id -> list<Task>. For the task taken as input and all other tasks in the current lane, build the map iteratively using a helper function called AddTasksToMap. This is a new helper function that takes as input a single task, a vector of PoolQuery objects, and a reference to the map. Iterate over the PoolQuery vector. In the loop, make a copy of the task using the container NewCopy method and then an entry in the unordered map. The node id should be taken from the PoolQuery. The PoolQuery should be unchanged from the iterator. The task should be the copy. 
4. When the unordered_map is built, we will iterate over the unordered map. We will create a TaskSaveIn that serializes each task in the list. For each task, since they are copies, update the pool query in the task to the resolved PoolQuery. We then call the container SaveIn method with the TaskSaveIn and task as inputs.

Here is the general flow of TaskSaveIn (let's call it ``ar``) in ClientSendTaskIn:
1. Take as input the number of tasks being serialized in its constructor. This will be serialized using cereal.
2. ClientSendTaskIn will do ``ar << (*task)``, which will call the task's SerializeIn method. If SerializeIn calls ar.bulk(), a DataTransfer object will be appended to a DataTransfer vector in the archive. In addition, the DataTransfer will be serialized. The DataTransfer should store a char* instead of hipc::Pointer. 
3. After all tasks are serialized, the serialized string as obtained from the archive and then transferred with lightbeam. Then, we iterate over each DataTransfer object and transfer each individually.

# ServerRecvTaskIn

1. Use lightbeam to receive a message
2. Deserialize the message using TaskLoadIn archive 

