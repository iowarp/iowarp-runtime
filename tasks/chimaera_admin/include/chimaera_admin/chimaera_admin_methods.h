#ifndef CHI_CHIMAERA_ADMIN_METHODS_H_
#define CHI_CHIMAERA_ADMIN_METHODS_H_

/** The set of methods in the admin task */
struct Method : public TaskMethod {
  TASK_METHOD_T kCreatePool = 10;
  TASK_METHOD_T kDestroyContainer = 11;
  TASK_METHOD_T kRegisterModule = 12;
  TASK_METHOD_T kDestroyModule = 13;
  TASK_METHOD_T kUpgradeModule = 14;
  TASK_METHOD_T kGetPoolId = 15;
  TASK_METHOD_T kStopRuntime = 16;
  TASK_METHOD_T kSetWorkOrchQueuePolicy = 17;
  TASK_METHOD_T kSetWorkOrchProcPolicy = 18;
  TASK_METHOD_T kFlush = 19;
  TASK_METHOD_T kGetDomainSize = 20;
  TASK_METHOD_T kUpdateDomain = 21;
  TASK_METHOD_T kPollStats = 22;
  TASK_METHOD_T kCount = 23;
};

#endif  // CHI_CHIMAERA_ADMIN_METHODS_H_