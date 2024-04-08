#ifndef HRUN_REMOTE_QUEUE_METHODS_H_
#define HRUN_REMOTE_QUEUE_METHODS_H_

/** The set of methods in the admin task */
struct Method : public TaskMethod {
  TASK_METHOD_T kClientPushSubmit = kLast + 0;
  TASK_METHOD_T kClientSubmit = kLast + 1;
  TASK_METHOD_T kServerPushComplete = kLast + 2;
  TASK_METHOD_T kServerComplete = kLast + 3;
};

#endif  // HRUN_REMOTE_QUEUE_METHODS_H_