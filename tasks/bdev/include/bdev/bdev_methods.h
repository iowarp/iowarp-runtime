#ifndef HRUN_BDEV_METHODS_H_
#define HRUN_BDEV_METHODS_H_

/** The set of methods in the admin task */
struct Method : public TaskMethod {
  TASK_METHOD_T kAllocate = kLast + 0;
  TASK_METHOD_T kFree = kLast + 1;
  TASK_METHOD_T kWrite = kLast + 2;
  TASK_METHOD_T kRead = kLast + 3;
};

#endif  // HRUN_BDEV_METHODS_H_