#include <easyforge/core/Jobs.h>

#include <deque>
#include <thread>

namespace easyforge
{
    namespace internal
    {
        class JobPool
        {
        public:
            explicit JobPool(int threadCount)
            {
                Threads.reserve(static_cast<std::size_t>(threadCount));
                for (int index = 0; index < threadCount; ++index)
                {
                    Threads.emplace_back([this] { Work(); });
                }
            }

            JobPool(const JobPool&) = delete;
            JobPool& operator=(const JobPool&) = delete;

            // Lets the waiting tasks finish, then stops the threads.
            ~JobPool()
            {
                {
                    std::lock_guard lock(Mutex);
                    Stopping = true;
                }
                Available.notify_all();

                for (std::thread& thread : Threads)
                {
                    // The last copy of a Jobs can be released inside one of its own
                    // jobs; a thread cannot wait for itself to end.
                    if (thread.get_id() == std::this_thread::get_id())
                    {
                        thread.detach();
                    }
                    else
                    {
                        thread.join();
                    }
                }
            }

            void Submit(Task task)
            {
                {
                    std::lock_guard lock(Mutex);
                    Waiting.push_back(std::move(task));
                }
                Available.notify_one();
            }

            bool RunOne()
            {
                Task task;
                {
                    std::lock_guard lock(Mutex);
                    if (Waiting.empty())
                    {
                        return false;
                    }
                    task = std::move(Waiting.front());
                    Waiting.pop_front();
                }
                task();
                return true;
            }

            int ThreadCount() const { return static_cast<int>(Threads.size()); }

        private:
            void Work()
            {
                for (;;)
                {
                    Task task;
                    {
                        std::unique_lock lock(Mutex);
                        Available.wait(lock, [this] { return Stopping || !Waiting.empty(); });
                        if (Waiting.empty())
                        {
                            return;
                        }
                        task = std::move(Waiting.front());
                        Waiting.pop_front();
                    }
                    task();
                }
            }

            std::mutex Mutex;
            std::condition_variable Available;
            std::deque<Task> Waiting;
            std::vector<std::thread> Threads;
            bool Stopping = false;
        };

        void Submit(JobPool& pool, Task task)
        {
            pool.Submit(std::move(task));
        }

        bool RunOneWaitingTask(JobPool& pool)
        {
            return pool.RunOne();
        }

        int ThreadCountOf(const JobPool& pool)
        {
            return pool.ThreadCount();
        }
    }

    namespace
    {
        int DefaultThreadCount()
        {
            int hardwareThreads = static_cast<int>(std::thread::hardware_concurrency());
            return Max(1, hardwareThreads - 1);
        }
    }

    Jobs Jobs::New(const Settings& settings)
    {
        int threadCount = settings.ThreadCount > 0 ? settings.ThreadCount : DefaultThreadCount();
        Jobs jobs;
        jobs.Pool = std::make_shared<internal::JobPool>(threadCount);
        return jobs;
    }

    Jobs Jobs::Shared()
    {
        static const std::shared_ptr<internal::JobPool> pool = std::make_shared<internal::JobPool>(DefaultThreadCount());
        Jobs jobs;
        jobs.Pool = pool;
        return jobs;
    }
}
