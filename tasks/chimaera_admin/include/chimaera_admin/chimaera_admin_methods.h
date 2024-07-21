#ifndef CHI_CHIMAERA_ADMIN_METHODS_H_
#define CHI_CHIMAERA_ADMIN_METHODS_H_

/** The set of methods in the admin task */
struct Method : public TaskMethod {
  TASK_METHOD_T kCreate = 0;
  TASK_METHOD_T kDestroy = 1;
  TASK_METHOD_T kCreateContainer = 10;
  TASK_METHOD_T kDestroyContainer = 11;
  TASK_METHOD_T kRegisterModule = 12;
  TASK_METHOD_T kDestroyModule = 13;
  TASK_METHOD_T kGetPoolId = 14;
  TASK_METHOD_T kStopRuntime = 15;
  TASK_METHOD_T kSetWorkOrchQueuePolicy = 16;
  TASK_METHOD_T kSetWorkOrchProcPolicy = 17;
  TASK_METHOD_T kFlush = 18;
  TASK_METHOD_T kGetDomainSize = 19;
  TASK_METHOD_T kUpdateDomain = 20;
};

#endif  // CHI_CHIMAERA_ADMIN_METHODS_H_