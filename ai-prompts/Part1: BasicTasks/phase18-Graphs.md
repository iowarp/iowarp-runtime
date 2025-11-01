@CLAUDE.md Let's add the concept of task graphs.

We will add a new method to the admin chimod called ProcessTaskGraph.

```cpp
struct TaskNode {
  chi::ipc::vector<Pointer> tasks_;
};
```

A task graph is a chi::ipc::vector<TaskNode> graph_.

Essentially, it is just a vector of tasks.