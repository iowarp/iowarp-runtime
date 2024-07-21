#ifndef CHI_REMOTE_QUEUE_METHODS_H_
#define CHI_REMOTE_QUEUE_METHODS_H_

/** The set of methods in the admin task */
struct Method : public TaskMethod {
  TASK_METHOD_T kCreate = 0;
  TASK_METHOD_T kDestroy = 1;
  TASK_METHOD_T kClientPushSubmit = 10;
  TASK_METHOD_T kClientSubmit = 11;
  TASK_METHOD_T kServerPushComplete = 12;
  TASK_METHOD_T kServerComplete = 13;
};

#endif  // CHI_REMOTE_QUEUE_METHODS_H_