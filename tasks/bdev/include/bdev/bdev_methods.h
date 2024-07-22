#ifndef CHI_BDEV_METHODS_H_
#define CHI_BDEV_METHODS_H_

/** The set of methods in the admin task */
struct Method : public TaskMethod {
  TASK_METHOD_T kAllocate = 10;
  TASK_METHOD_T kFree = 11;
  TASK_METHOD_T kWrite = 12;
  TASK_METHOD_T kRead = 13;
};

#endif  // CHI_BDEV_METHODS_H_