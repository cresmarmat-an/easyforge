#include <easyforge/core.h>
#include <easyforge/core/Testing.h>

#include <atomic>
#include <memory>
#include <vector>

using namespace easyforge;

EASYFORGE_TEST(JobsReturnValues)
{
    Jobs jobs = Jobs::New({ .ThreadCount = 2 });
    EASYFORGE_REQUIRE(jobs);
    EASYFORGE_EXPECT_EQUAL(jobs.ThreadCount(), 2);

    Job<int> answer = jobs.Run([] { return 6 * 7; });
    EASYFORGE_EXPECT_EQUAL(answer.Get(), 42);
    EASYFORGE_EXPECT(answer.IsDone());
}

EASYFORGE_TEST(JobsRunEveryJob)
{
    Jobs jobs = Jobs::New({ .ThreadCount = 4 });
    std::atomic<int> total = 0;

    std::vector<Job<>> started;
    for (int index = 1; index <= 100; ++index)
    {
        started.push_back(jobs.Run([&total, index] { total += index; }));
    }
    for (const Job<>& job : started)
    {
        job.Wait();
    }

    EASYFORGE_EXPECT_EQUAL(total.load(), 5050);
}

EASYFORGE_TEST(JobsMoveOnlyFunction)
{
    Jobs jobs = Jobs::New({ .ThreadCount = 1 });
    auto owned = std::make_unique<int>(9);
    Job<int> job = jobs.Run([owned = std::move(owned)] { return *owned + 1; });
    EASYFORGE_EXPECT_EQUAL(job.Get(), 10);
}

EASYFORGE_TEST(JobsWaitingInsideAJobDoesNotFreeze)
{
    // With one thread, the outer job occupies it. Waiting on the inner job only
    // finishes because Wait runs waiting jobs itself.
    Jobs jobs = Jobs::New({ .ThreadCount = 1 });
    Job<int> outer = jobs.Run([jobs] {
        Job<int> inner = jobs.Run([] { return 5; });
        return inner.Get() * 2;
    });
    EASYFORGE_EXPECT_EQUAL(outer.Get(), 10);
}

EASYFORGE_TEST(JobsParallelForVisitsEachIndexOnce)
{
    Jobs jobs = Jobs::New({ .ThreadCount = 3 });

    constexpr int count = 1000;
    std::vector<std::atomic<int>> visits(count);
    jobs.ParallelFor(count, [&visits](int index) { visits[static_cast<std::size_t>(index)] += 1; });

    bool eachOnce = true;
    for (const std::atomic<int>& visit : visits)
    {
        eachOnce = eachOnce && visit.load() == 1;
    }
    EASYFORGE_EXPECT(eachOnce);

    int calls = 0;
    jobs.ParallelFor(0, [&calls](int) { ++calls; });
    EASYFORGE_EXPECT_EQUAL(calls, 0);
}

EASYFORGE_TEST(JobsSharedIsTheSamePool)
{
    Jobs first = Jobs::Shared();
    Jobs second = Jobs::Shared();
    EASYFORGE_REQUIRE(first && second);
    EASYFORGE_EXPECT(first.ThreadCount() >= 1);
    EASYFORGE_EXPECT_EQUAL(first.Run([] { return 1; }).Get() + second.Run([] { return 2; }).Get(), 3);
}

EASYFORGE_TEST(JobsEmptyJobIsDone)
{
    Job<> never;
    EASYFORGE_EXPECT(never.IsDone());
    never.Wait();
    EASYFORGE_EXPECT(!Jobs {});
}
