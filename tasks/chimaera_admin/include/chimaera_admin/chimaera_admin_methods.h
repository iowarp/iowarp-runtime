#ifndef CHI_CHIMAERA_ADMIN_METHODS_H_
#define CHI_CHIMAERA_ADMIN_METHODS_H_

/** The set of methods in the admin task */
struct Method : public TaskMethod {
  TASK_METHOD_T kCreatePool = 10;
  TASK_METHOD_T kDestroyPool = 11;
  TASK_METHOD_T kStopRuntime = 12;
  TASK_METHOD_T kCount = 13;
};

#endif  // CHI_CHIMAERA_ADMIN_METHODS_H_