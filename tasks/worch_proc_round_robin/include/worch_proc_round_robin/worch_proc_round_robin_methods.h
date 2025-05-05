#ifndef CHI_WORCH_PROC_ROUND_ROBIN_METHODS_H_
#define CHI_WORCH_PROC_ROUND_ROBIN_METHODS_H_

/** The set of methods in the admin task */
struct Method : public chi::TaskMethod {
  TASK_METHOD_T kSchedule = 10;
  TASK_METHOD_T kCount = 11;
};

#endif  // CHI_WORCH_PROC_ROUND_ROBIN_METHODS_H_