All code for creating new tasks in include/chimaera/client/chimaera_client.h should be moved to include/chimaera/ipc/ipc_manager.h. Delete chimaera_client.h

Remove ChimaeraRuntime class and replace with a function CHIMAERA_RUNTIME_INIT.

Implement functions named CHIMAERA_CLIENT_INIT and CHIMAERA_RUNTIME_INIT in include/chimaera/chimaera.h that initializes IPC manager for the client.

Create a new class called Chimaera with methods ClientInit and ServerInit in include/chimaera/chimaera.h. Include singleton for this class using hermes shm. Implement the methods in the corresponding source file. CHIMAERA_CLIENT_INIT and CHIMAERA_RUNTIME_INIT should call the singletons.