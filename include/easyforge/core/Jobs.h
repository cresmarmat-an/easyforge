#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <easyforge/core/Scalar.h>

namespace easyforge
{
    class Jobs;

    namespace internal
    {
        // A function that runs once. Unlike std::function it can hold lambdas
        // that can only be moved, such as ones that own a std::unique_ptr.
        class Task
        {
        public:
            Task() = default;

            template <typename Function>
            explicit Task(Function function) : Callable(std::make_unique<Holder<Function>>(std::move(function)))
            {
            }

            void operator()() { Callable->Run(); }

        private:
            struct Base
            {
                virtual ~Base() = default;
                virtual void Run() = 0;
            };

            template <typename Function>
            struct Holder final : Base
            {
                explicit Holder(Function function) : Stored(std::move(function)) {}
                void Run() override { Stored(); }
                Function Stored;
            };

            std::unique_ptr<Base> Callable;
        };

        class JobPool;

        void Submit(JobPool& pool, Task task);

        // Runs one waiting task on the calling thread. False when none was waiting.
        bool RunOneWaitingTask(JobPool& pool);

        int ThreadCountOf(const JobPool& pool);

        template <typename Type>
        struct JobState
        {
            using Stored = std::conditional_t<std::is_void_v<Type>, std::monostate, Type>;

            std::mutex Mutex;
            std::condition_variable Finished;
            std::atomic<bool> Done = false;
            std::optional<Stored> Value;
        };
    }

    // Work running in the background. Returned by Jobs::Run.
    template <typename Type = void>
    class Job
    {
    public:
        // A job that was never started. It counts as done.
        Job() = default;

        // False for a job made with Job(), which has no function and no value.
        bool IsStarted() const { return State != nullptr; }

        bool IsDone() const { return !State || State->Done.load(std::memory_order_acquire); }

        // Returns once the job has finished. While waiting, this thread runs other
        // waiting jobs, so a job can wait for another job without freezing the pool.
        void Wait() const
        {
            if (!State)
            {
                return;
            }
            while (!IsDone())
            {
                if (!internal::RunOneWaitingTask(*Pool))
                {
                    std::unique_lock lock(State->Mutex);
                    State->Finished.wait_for(lock, std::chrono::milliseconds(1), [this] { return IsDone(); });
                }
            }
        }

        // Waits, then gives the value the job's function returned.
        std::add_lvalue_reference_t<const Type> Get() const
            requires(!std::is_void_v<Type>)
        {
            Wait();
            return *State->Value;
        }

    private:
        friend class Jobs;

        std::shared_ptr<internal::JobPool> Pool;
        std::shared_ptr<internal::JobState<Type>> State;
    };

    // A group of threads that run functions in the background.
    //
    //     Jobs jobs = Jobs::Shared();
    //     Job<int> answer = jobs.Run([] { return 6 * 7; });
    //     int value = answer.Get();
    //
    // A job's function must not throw an exception. Copies of a Jobs refer to the
    // same threads, which finish the jobs still waiting and stop when the last
    // copy is gone.
    class Jobs
    {
    public:
        struct Settings
        {
            // How many threads to start. 0 means one fewer than the processor's
            // hardware threads, and at least one.
            int ThreadCount = 0;
        };

        static Jobs New(const Settings& settings = {});

        // Threads shared by the whole program, started the first time this is called.
        static Jobs Shared();

        // A Jobs made by Jobs() has no threads; New or Shared make usable ones.
        Jobs() = default;

        explicit operator bool() const { return Pool != nullptr; }

        int ThreadCount() const { return Pool ? internal::ThreadCountOf(*Pool) : 0; }

        template <typename Function>
        auto Run(Function function) const -> Job<std::invoke_result_t<Function&>>
        {
            using Type = std::invoke_result_t<Function&>;

            auto state = std::make_shared<internal::JobState<Type>>();
            internal::Submit(*Pool, internal::Task([state, function = std::move(function)]() mutable {
                if constexpr (std::is_void_v<Type>)
                {
                    function();
                    state->Value.emplace();
                }
                else
                {
                    state->Value.emplace(function());
                }
                {
                    std::lock_guard lock(state->Mutex);
                    state->Done.store(true, std::memory_order_release);
                }
                state->Finished.notify_all();
            }));

            Job<Type> job;
            job.Pool = Pool;
            job.State = std::move(state);
            return job;
        }

        // Calls `function(index)` for every index from 0 to count - 1, split across
        // the threads and the calling thread, and returns when all calls are done.
        // Calls run at the same time, so `function` must be safe to call from
        // several threads at once.
        template <typename Function>
        void ParallelFor(int count, Function function) const
        {
            if (count <= 0)
            {
                return;
            }

            int pieceCount = Min(count, (ThreadCount() + 1) * 4);
            int pieceSize = (count + pieceCount - 1) / pieceCount;

            std::vector<Job<>> started;
            started.reserve(static_cast<std::size_t>(pieceCount));
            for (int begin = pieceSize; begin < count; begin += pieceSize)
            {
                int end = Min(count, begin + pieceSize);
                started.push_back(Run([&function, begin, end] {
                    for (int index = begin; index < end; ++index)
                    {
                        function(index);
                    }
                }));
            }

            for (int index = 0; index < Min(count, pieceSize); ++index)
            {
                function(index);
            }

            for (const Job<>& job : started)
            {
                job.Wait();
            }
        }

    private:
        std::shared_ptr<internal::JobPool> Pool;
    };
}
