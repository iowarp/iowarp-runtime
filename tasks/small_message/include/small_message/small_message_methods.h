#ifndef CHI_SMALL_MESSAGE_METHODS_H_
#define CHI_SMALL_MESSAGE_METHODS_H_

/** The set of methods in the admin task */
struct Method : public TaskMethod {
  TASK_METHOD_T kMd = 10;
  TASK_METHOD_T kIo = 11;
  TASK_METHOD_T kCount = 12;
};

#endif  // CHI_SMALL_MESSAGE_METHODS_H_