#ifndef CHI_TASK_NAME_METHODS_H_
#define CHI_TASK_NAME_METHODS_H_

/** The set of methods in the admin task */
struct Method : public TaskMethod {
  TASK_METHOD_T kCustom = 10;
  TASK_METHOD_T kCount = 11;
};

#endif  // CHI_TASK_NAME_METHODS_H_