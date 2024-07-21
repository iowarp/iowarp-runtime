#ifndef CHI_SMALL_MESSAGE_METHODS_H_
#define CHI_SMALL_MESSAGE_METHODS_H_

/** The set of methods in the admin task */
struct Method : public TaskMethod {
  TASK_METHOD_T kCreate = 0;
  TASK_METHOD_T kDestroy = 1;
  TASK_METHOD_T kMd = 10;
  TASK_METHOD_T kIo = 11;
};

#endif  // CHI_SMALL_MESSAGE_METHODS_H_