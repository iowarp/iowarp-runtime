#ifndef HRUN_CHIMAERA_ADMIN_METHODS_H_
#define HRUN_CHIMAERA_ADMIN_METHODS_H_

/** The set of methods in the admin task */
struct Method : public TaskMethod {
  TASK_METHOD_T kCreateTaskState = kLast + 0;
  TASK_METHOD_T kDestroyTaskState = kLast + 1;
  TASK_METHOD_T kRegisterTaskLib = kLast + 2;
  TASK_METHOD_T kDestroyTaskLib = kLast + 3;
  TASK_METHOD_T kGetOrCreateTaskStateId = kLast + 4;
  TASK_METHOD_T kGetTaskStateId = kLast + 5;
  TASK_METHOD_T kStopRuntime = kLast + 6;
  TASK_METHOD_T kSetWorkOrchQueuePolicy = kLast + 7;
  TASK_METHOD_T kSetWorkOrchProcPolicy = kLast + 8;
  TASK_METHOD_T kFlush = kLast + 9;
  TASK_METHOD_T kGetDomainSize = kLast + 10;
  TASK_METHOD_T kCreateDomain = kLast + 11;
  TASK_METHOD_T kGetDomain = kLast + 12;
  TASK_METHOD_T kUpdateDomain = kLast + 13;
};

#endif  // HRUN_CHIMAERA_ADMIN_METHODS_H_