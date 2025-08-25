## Code Style

Use the Google C++ style guide for C++.

You should store the pointer returned by the singleton GetInstance method. Avoid dereferencing GetInstance method directly using either -> or *. E.g., do not do ``hshm::Singleton<T>::GetInstance()->var_``. You should do ``auto *x = hshm::Singleton<T>::GetInstance(); x->var_;``.

## Workflow
Use the incremental logic builder agent if you are making code changes.

Use the compiler subagent for making changes to cmakes and identifying places that need to be fixed in the code.

Always verify that code continue to compiles after making changes.

Whenever building unit tests, make sure to use the unit testing agent.

Whenvere performing filesystem queries or executing programs, use the filesystem ops script agent.