#include <atomic>
#include <numeric>
#include <thread>
#include <vector>

#include <easyforge/core/Jobs.h>
#include <easyforge/core/Testing.h>

using namespace easyforge;

// Jobs started from many threads at once, jobs that start and wait on more
// jobs, and pools let go of while they work, all without losing or repeating a
// job, freezing, or touching freed memory.

EASYFORGE_TEST(JobsFromManyThreadsAllRunOnce)
{
    Jobs jobs = Jobs::New({ .ThreadCount = 4 });
    std::atomic<long long> total { 0 };
    std::atomic<long long> returned { 0 };
    std::vector<std::thread> starters;
    for (int starter = 0; starter < 6; ++starter)
    {
        starters.emplace_back([&, starter] {
            std::vector<Job<int>> started;
            for (int index = 0; index < 1500; ++index)
            {
                int value = starter * 10000 + index;
                started.push_back(jobs.Run([value, &total] {
                    total += value;
                    return value;
                }));
            }
            for (const Job<int>& job : started)
            {
                returned += job.Get();
            }
        });
    }
    for (std::thread& starter : starters)
    {
        starter.join();
    }
    long long expected = 0;
    for (int starter = 0; starter < 6; ++starter)
    {
        for (int index = 0; index < 1500; ++index)
        {
            expected += starter * 10000 + index;
        }
    }
    EASYFORGE_EXPECT_EQUAL(total.load(), expected);
    EASYFORGE_EXPECT_EQUAL(returned.load(), expected);
}

EASYFORGE_TEST(JobsThatWaitOnJobsNeverFreeze)
{
    // Fewer threads than jobs waiting, so waiting jobs must run others.
    Jobs jobs = Jobs::New({ .ThreadCount = 2 });
    std::vector<Job<int>> outer;
    for (int index = 0; index < 64; ++index)
    {
        outer.push_back(jobs.Run([&jobs, index] {
            std::vector<Job<int>> inner;
            for (int part = 0; part < 8; ++part)
            {
                inner.push_back(jobs.Run([index, part] { return index + part; }));
            }
            int sum = 0;
            for (const Job<int>& job : inner)
            {
                sum += job.Get();
            }
            std::atomic<int> visited { 0 };
            jobs.ParallelFor(100, [&visited](int) { ++visited; });
            return sum + visited.load();
        }));
    }
    for (int index = 0; index < 64; ++index)
    {
        EASYFORGE_EXPECT_EQUAL(outer[static_cast<std::size_t>(index)].Get(), index * 8 + 28 + 100);
    }
}

EASYFORGE_TEST(PoolsLetGoOfWhileWorkingFinishTheirJobs)
{
    for (int round = 0; round < 20; ++round)
    {
        std::atomic<int> ran { 0 };
        std::vector<Job<void>> started;
        {
            Jobs jobs = Jobs::New({ .ThreadCount = 3 });
            for (int index = 0; index < 200; ++index)
            {
                started.push_back(jobs.Run([&ran] { ++ran; }));
            }
        }
        // The handle is gone; the jobs it started still finish.
        for (const Job<void>& job : started)
        {
            job.Wait();
        }
        EASYFORGE_EXPECT_EQUAL(ran.load(), 200);
    }
}
