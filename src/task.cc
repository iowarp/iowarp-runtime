/**
 * Task implementation
 */

#include "chimaera/task.h"

namespace chi {

// Default constructor (protected)
Task::Task() : hipc::ShmContainer(), pool_id_(0), task_node_(0),
               dom_query_(), method_(0), task_flags_(0), period_ns_(0.0) {
}

Task::~Task() {
  // Stub destructor
}

void Task::Wait() {
  // Stub implementation - would normally wait for task completion
}

void Task::Wait(Task* subtask) {
  // Stub implementation - would normally wait for specific subtask
  if (subtask != nullptr) {
    subtask->Wait();
  }
}

bool Task::IsPeriodic() const {
  return task_flags_.Any(TASK_PERIODIC);
}

bool Task::IsFireAndForget() const {
  return task_flags_.Any(TASK_FIRE_AND_FORGET);
}

double Task::GetPeriod() const {
  return period_ns_;
}

void Task::SetFlags(u32 flags) {
  task_flags_.SetBits(flags);
}

void Task::ClearFlags(u32 flags) {
  task_flags_.UnsetBits(flags);
}

// Template instantiation removed - constructor is now inline in header

}  // namespace chi