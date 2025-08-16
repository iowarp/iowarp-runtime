### Locking and Synchronization

Implement a custom mutex and reader-writer lock for the chimeara runtime. Modules should use these locks internally as much as possible, instead of std::mutex. Use the TaskNode property to detect if a particular group of tasks are holding the lock. If two tasks with the same major ID attempt to hold the lock, then grant them access. Otherwise, store a table mapping major id to a vector of tasks with the same major. When unlocked, choose a particular major group and then reschedule all tasks in that group.
