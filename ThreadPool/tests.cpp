#include <algorithm>
#include <gtest/gtest.h>

#include "ThreadPool.h"

template<typename T>
class ThreadPoolTest : public ::testing::Test {
public:
    ThreadPoolTest() : pool(4) {}
protected:
    ThreadPool<T::value> pool;
};

using WorkSharingStrategy = std::integral_constant<Strategy, Strategy::WorkSharing>;
using WorkStealingStrategy = std::integral_constant<Strategy, Strategy::WorkStealing>;

typedef ::testing::Types<WorkSharingStrategy, WorkStealingStrategy> Implementations;

TYPED_TEST_SUITE(ThreadPoolTest, Implementations);

TYPED_TEST(ThreadPoolTest, SingleTask)
{
    std::future<int> result = this->pool.enqueue([]() { return 1; });
    ASSERT_EQ(result.get(), 1);
}

TYPED_TEST(ThreadPoolTest, MultipleTasks)
{
    std::future<int> result1 = this->pool.enqueue([]() { return 1; });
    std::future<int> result2 = this->pool.enqueue([]() { return 2; });
    ASSERT_EQ(result1.get() + result2.get(), 3);
}

template <Strategy strategy> void test_high_volume(ThreadPool<strategy> pool)
{
    std::vector<std::future<int>> results;
    for (int i = 0; i < 1000; ++i) {
        results.push_back(pool.enqueue([i]() { return i; }));
    }
    int sum = 0;
    for (auto &result : results) {
        sum += result.get();
    }
    ASSERT_EQ(sum, 499500); // Sum of first 1000 natural numbers
}

// This test do not use default 4 pool size
TEST(ThreadPoolTests, HighVolume)
{
    test_high_volume(ThreadPool<Strategy::WorkSharing>(100));
    test_high_volume(ThreadPool<Strategy::WorkStealing>(100));
}

// Task with side-effects
TYPED_TEST(ThreadPoolTest, SideEffectTask)
{
    std::vector<int> numbers(1000, 0);
    std::vector<std::future<void>> futures;

    for (int i = 0; i < 1000; ++i) {
        futures.push_back(
            this->pool.enqueue([&numbers, i]() { numbers[i] = i; }));
    }

    for (auto &future : futures) {
        future.get();
    }

    // Check if numbers vector was populated correctly
    bool all_correct =
        std::all_of(numbers.begin(), numbers.end(),
                    [idx = 0](int value) mutable { return value == idx++; });
    ASSERT_TRUE(all_correct);
}

// Tasks with varying execution times
TYPED_TEST(ThreadPoolTest, VaryingExecutionTimes)
{
    std::vector<std::future<int>> futures;

    for (int i = 0; i < 10; ++i) {
        futures.push_back(this->pool.enqueue([i]() {
            // Increasing sleep time
            std::this_thread::sleep_for(std::chrono::milliseconds(50 * i));
            return i;
        }));
    }

    int sum = 0;
    for (auto &future : futures) {
        sum += future.get();
    }

    ASSERT_EQ(sum, 45); // Sum of first 10 natural numbers (0-9)
}

// Tasks that throw exceptions
TYPED_TEST(ThreadPoolTest, ExceptionTask)
{
    std::future<void> result = this->pool.enqueue(
        []() { throw std::runtime_error("Test exception"); });

    EXPECT_THROW(
        {
            try {
                result.get();
            } catch (const std::runtime_error &e) {
                ASSERT_STREQ("Test exception", e.what());
                throw;
            }
        },
        std::runtime_error);
}

// Tasks returning complex data types
TYPED_TEST(ThreadPoolTest, ReturnComplexType)
{
    std::future<std::vector<int>> result =
        this->pool.enqueue([]() -> std::vector<int> {
            return std::vector<int>{1, 2, 3, 4, 5};
        });

    std::vector<int> expected{1, 2, 3, 4, 5};
    ASSERT_EQ(result.get(), expected);
}

TYPED_TEST(ThreadPoolTest, ThreadCount)
{
    std::atomic<int> counter{0};
    for (int i = 0; i < 100; ++i) {
        this->pool.enqueue([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
    }
    // Wait for some tasks to complete
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // This tests that at least 4 tasks have been processed concurrently
    ASSERT_GE(counter.load(), 4);
}

// Tasks with dependencies (simulate a simple continuation, but not use continue_with)
TYPED_TEST(ThreadPoolTest, TaskDependencies)
{
    std::future<int> initial_task = this->pool.enqueue([]() { return 42; });

    std::future<std::string> dependent_task =
        this->pool.enqueue([&initial_task]() {
            // This will wait for the initial task to complete
            int result = initial_task.get();
            return std::string("Result is ") + std::to_string(result);
        });

    ASSERT_EQ(dependent_task.get(), "Result is 42");
}


// Test continuation with simple types
TYPED_TEST(ThreadPoolTest, ContinueWithSimpleType)
{
    auto initial_task = this->pool.enqueue([]() { return 42; });

    auto continuation_task = this->pool.continue_with(
        std::move(initial_task), [](int result) -> std::string {
            return std::to_string(result) + " is the answer";
        });

    ASSERT_EQ(continuation_task.get(), "42 is the answer");
}

// Test chaining multiple continuations
TYPED_TEST(ThreadPoolTest, ChainMultipleContinuations)
{
    auto initial_task = this->pool.enqueue([]() { return 1; });

    auto first_continuation_task =
        this->pool.continue_with(std::move(initial_task), [](int result) {
            return result + 1; // Increment the result
        });

    auto second_continuation_task = this->pool.continue_with(
        std::move(first_continuation_task), [](int result) {
            return result * 2; // Double the result
        });

    ASSERT_EQ(second_continuation_task.get(), 4); // Expected result is 4
}

// Test continuation with complex types
TYPED_TEST(ThreadPoolTest, ContinueWithComplexType)
{
    std::future<std::vector<int>> initial_task =
        this->pool.enqueue([]() -> std::vector<int> {
            return {1, 2, 3, 4, 5};
        });

    auto continuation_task = this->pool.continue_with(
        std::move(initial_task), [](const std::vector<int> &vec) {
            // Sum the vector elements
            return std::accumulate(vec.begin(), vec.end(), 0);
        });

    ASSERT_EQ(continuation_task.get(), 15); // Sum of 1+2+3+4+5
}
