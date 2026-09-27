# Background jobs

`Jobs` runs functions on other threads, so slow work such as loading or
generating does not hold up the rest of the program.

```cpp
#include <easyforge/core.h>

using namespace easyforge;

Jobs jobs = Jobs::Shared();

Job<int> lineCount = jobs.Run([] { return CountLinesInBigFile(); });

// ... other work while it runs ...

if (lineCount.IsDone())
{
    Log("{} lines", lineCount.Get());
}
```

## Getting threads

| Written | Result |
|---|---|
| `Jobs::Shared()` | Threads shared by the whole program, started the first time it is called |
| `Jobs::New({ .ThreadCount = 4 })` | Your own group of threads |
| `Jobs::New()` | Your own group, with one fewer thread than the processor has hardware threads, and at least one |
| `jobs.ThreadCount()` | How many threads it has |
| `if (jobs)` | False for a `Jobs` made with `Jobs {}`, which has no threads |

Most programs only need `Jobs::Shared()`. Make your own group when some work
must never wait behind other work, for example a group of one thread that only
streams music.

A `Jobs` is a handle: copies share the same threads. The threads finish the jobs
still waiting and stop when the last copy is gone. The shared threads stop when
the program ends.

## Running a job

`jobs.Run(function)` starts `function` on one of the threads and returns a
`Job<T>`, where `T` is what the function returns (`Job<>` for `void`).

| Written | Result |
|---|---|
| `job.IsDone()` | True once the function has returned |
| `job.Wait()` | Returns once the function has returned |
| `job.Get()` | Waits, then gives the value the function returned |

Jobs start in the order they were given. The function can be a lambda that
owns things which can only be moved, such as a `std::unique_ptr`.

## Waiting without freezing

While `Wait` or `Get` is waiting, the waiting thread runs other waiting jobs
itself. So a job can start another job and wait for it, even when every thread
is busy, without the whole group stopping:

```cpp
Job<int> outer = jobs.Run([jobs] {
    Job<int> inner = jobs.Run([] { return 5; });
    return inner.Get() * 2;
});
```

## Splitting a loop across threads

```cpp
std::vector<Vector3> positions = ...;
jobs.ParallelFor(static_cast<int>(positions.size()), [&positions](int index) {
    positions[index] = Wobble(positions[index]);
});
```

`ParallelFor(count, function)` calls `function(index)` once for every index from
0 to `count - 1`, spread across the threads and the calling thread, and returns
when every call is done. The calls run at the same time, so the function must be
safe to call from several threads at once: writing to a different element for
each index, as above, is safe.

## Rules

- A job's function must not throw. An exception that escapes a job ends the
  program.
- A job must not assume which thread it runs on, or that it runs right away.
- Anything a job uses must stay alive until the job is done. Capture by value, or
  wait for the job before the things it uses go away.

## Limitations

- There are no priorities; jobs start in the order they were given.
- A job cannot be cancelled once given. Have the function check a flag of your
  own if it may need to stop early.
- Jobs wait in one shared list guarded by one lock. That suits jobs that take at
  least a fraction of a millisecond; millions of tiny jobs would spend their time
  waiting for the lock. `ParallelFor` groups indices into a few pieces per
  thread for this reason.
