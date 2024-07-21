#ifndef CHI_WORCH_QUEUE_ROUND_ROBIN_METHODS_H_
#define CHI_WORCH_QUEUE_ROUND_ROBIN_METHODS_H_

/** The set of methods in the admin task */
struct Method : public TaskMethod {
  TASK_METHOD_T kCreate = 0;
  TASK_METHOD_T kDestroy = 1;
  TASK_METHOD_T kSchedule = 10;
};

#endif  // CHI_WORCH_QUEUE_ROUND_ROBIN_METHODS_H_