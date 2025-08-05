Implement empty macros CHI_BEGIN(X) and CHI_END(X). They are simply decorators to help with code generation.

Implement src/CMakeLists.txt to compile the chimaera runtime and chimaera client. There should be separate targets for them.
Also create aliases for them with the chi:: namespace.  Ensure that CHIMAERA_RUNTIME macro is set for runtime code. Make sure to link to hermes_shm. It has the targets hshm::cxx, hshm::cudacxx and hshm::rocmcxx_gpu. For now, use only hshm::cxx.

In the modules, remove CHI_NAMESPACE_INIT from the chimods in tasks. In addition, ensure that *tasks.h include the chimeara_admin.h, which is the client code for the admin. Protect this under the header guard CHI_TASKS_CHI_ADMIN_INCLUDE_CHI_ADMIN_CHI_ADMIN_TASKS_H_ to avoid a cyclic dependency. 

Include chimaera.h in each of the modules under tasks. Remove all headers that do not exist. Don't include repetitive headers.


#include <hermes_shm/util/shared_library.h> is not real. The path is <hermes_shm/introspect/system_info.h>