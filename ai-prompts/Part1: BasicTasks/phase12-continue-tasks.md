@CLAUDE.md

ContinueBlockedTasks is prone to deadlocking. If a task becomes blocked again during ContinueBlockedTasks,
it will keep rechecking the same task. We should store two blocked queues and then swap which one gets iterated.
Every time ContineuBlockedTasks starts, we flip a boolean so that subsequent blocking operations go to the other queue.

ExecTask should return a boolean indicating whether or not the task should have been completed. 

```
auto &orig_queue = blocked_queue_[block_queue_bit_];
block_queue_bit_ = !block_queue_bit_;
while(orig_queue):
  Pop the run context
  is_complete_ = ExecTask
  If so, continue
  Otherwise, decrement the remaining time by the current elapsed.
```

