
### Admin ChiMod

This is a special chimod that the chimaera runtime should always find. If it is not found, then a fatal error should occur. This chimod is responsible for creating chipools, destroying them, and stopping the runtime. Processes initially send tasks containing the parameters to the chimod they want to instantiate to the admin chimod, which then distributes the chipool. It should use the PoolManager singleton to create containers locally. The chimod has three main tasks:
1. CreatePool
2. DestroyPool
3. StopRuntime

When creating a container, a table should be built mapping DomainIds to either node ids or other DomainIds. These are referred to as domain tables. These tables should be stored as part of the pool metadata in PoolInfo. Two domains should be stored: kLocal and kGlobal. Local domain maps containers on this node to the global DomainId. Global maps DomainId to physical DomainIds, representing node Ids. The global domain table should be consistent across all nodes. 
