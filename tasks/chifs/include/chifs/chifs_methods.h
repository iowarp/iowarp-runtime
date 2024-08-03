#ifndef CHI_CHIFS_METHODS_H_
#define CHI_CHIFS_METHODS_H_

/** The set of methods in the admin task */
struct Method : public TaskMethod {
  TASK_METHOD_T kOpenFile = 10;
  TASK_METHOD_T kReadFile = 11;
  TASK_METHOD_T kWriteFile = 12;
  TASK_METHOD_T kAppendFile = 13;
  TASK_METHOD_T kDestroyFile = 14;
  TASK_METHOD_T kStatFile = 15;
  TASK_METHOD_T kCount = 16;
};

#endif  // CHI_CHIFS_METHODS_H_