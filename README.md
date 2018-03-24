# cppconcur
###To avoid deadlock:###
- Don't wait for another thread if there's a chance it's waiting for you.
- Don't acquire a lock if you already hold one.If need,use single std::lock.
- Avoid calling user-supplied code while holding a lock.
- if you have to lock several mutexes in seperate steps, do them in the same order.
- use hierarchical mutexes

In general,a lock should be held for only the minimum possible time needed to perform the required operations.
if don't hold the required locks for the entire duration of an operation,you're exposing yourself in race conditions.
