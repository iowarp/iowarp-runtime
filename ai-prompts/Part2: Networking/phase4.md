# Domain Resolution
We now need to focus on distributed scheduling. We can assume that a task has a DomainQuery object representing how to distribute the task among the pool. Right now, we have several options such as send to local container, directly hashing to a container, and broadcasting across all containers.


## Resolution Algorithm:
First check if GetDynamic was used in the DomainQuery. If so, then get the local container and call the Monitor function using the MonitorMode kGlobalSchedule. This will replace the domain query with something more concrete.

Roughly, the algorithm should have this prototype. It the container is already known, then it should be passed as a parameter too. Check worker.cc if this is the case.
```cpp
std::vector<ResolvedDomainQuery> ResolveDomain(const DomainQuery &query)
```

The resolved domain query should be stored in the RuntimeContext for the task. 

### Case 1: The task is hashed to a container
We locate the domain table for the pool. 
We then hash by module number containers get the container ID.
We then get the node ID that container is located on.
We update the domain query to be a direct route to that physical address.

### Case 2: The task is directed to a specific container
Same as case 1. 

### Case 3: The task is broadcasted across all containers
We divide the DomainQuery into smaller DomainQueries. There should be a configurable maximum number of DomainQueries produced. For now, let's say 16. If there are 256 containers, then there will be 16 DomainQueries produced, each that broadcasts to a subset of those containers. 

The node chosen for each resolved query is the physical address of the first node in each container. If it fails to send there, then it will retry with the next container. If all containers fail, that is fine.

This aims to avoid overwhelming nodes with tasks.

### Case 4: The task is broadcasted to a range of containers

Similar to case 3, it should map to smaller domain queries. DomainQuery object should have a "range" mode where a linear range of container IDs can be chosen for broadcast.

## Worker Route
If the ResolvedDomainQuery object exactly one entry and the resolved node ID is this node, then we schedule the task as-is. Otherwise, the task is sent to the chimaera admin using the ClientSendTask method. The DomainQuery used should be LocalHash.

Otherwise, the task should be scheduled like it is now.
