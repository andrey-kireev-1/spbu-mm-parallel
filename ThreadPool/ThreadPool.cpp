#include "ThreadPool.h"

/* Returns true with 1/n probability */
bool get_random_with_probability(size_t n) {
    if (n == 0)
        return true;
    thread_local std::mt19937 generator(std::random_device{}());
    thread_local std::uniform_int_distribution<size_t> distribution(1, n);
    return distribution(generator) == 1;
}

template <Strategy strategy>
void ThreadPool<strategy>::balance_queues(size_t curr, size_t victim) {
    std::scoped_lock lock(tasks_[curr].get_mutex(), tasks_[victim].get_mutex());

    auto &smaller_queue = tasks_[curr].unsafe_size() < tasks_[victim].unsafe_size() ? tasks_[curr] : tasks_[victim];
    auto &larger_queue = tasks_[curr].unsafe_size() < tasks_[victim].unsafe_size() ? tasks_[victim] : tasks_[curr];

    // Ensuring at most threshold_ task difference
    while (larger_queue.unsafe_size() - smaller_queue.unsafe_size() > threshold_) {
        if (auto task = larger_queue.unsafe_dequeue()) {
            smaller_queue.unsafe_enqueue(std::move(*task));
        }
    }
}

template <Strategy strategy> void ThreadPool<strategy>::worker_thread(size_t index)
{
    while (!stop_flag_.test(std::memory_order_relaxed)) {
        std::function<void()> task;
        bool found = false;

        // Try own queue first
        auto opt_task = tasks_[index].dequeue();
        if (opt_task.has_value()) {
            task = *opt_task;
            found = true;
        } else if constexpr (strategy == Strategy::WorkSharing) {
            size_t own_queue_size = tasks_[index].size();
            bool should_rebalance = get_random_with_probability(own_queue_size);

            if (should_rebalance) {
                size_t victim = get_rng_index();
                if (victim != index) {
                    balance_queues(index, victim);
                }
            }
        } else if constexpr (strategy == Strategy::WorkStealing) {
            size_t workers_size = workers_size_.load(std::memory_order_relaxed);
            for (size_t i = 0; i < workers_size; ++i) {
                auto opt_task_steal = tasks_[get_rng_index()].dequeue_top();
                if (opt_task_steal.has_value()) {
                    task = *opt_task_steal;
                    found = true;
                    break;
                }
            }
        }

        if (found) {
            task();
        } else {
            // No task was found, and this thread will sleep for a short
            // duration to reduce busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

template <Strategy strategy>
ThreadPool<strategy>::ThreadPool(size_t threads)
        : tasks_(threads), start_barrier_(threads + 1) // +1 for the main thread
{
    workers_size_.store(threads, std::memory_order_relaxed);

    for (size_t i = 0; i < threads; ++i)
        workers_.emplace_back([this, i] {
            // Wait for all threads to be ready
            start_barrier_.arrive_and_wait();
            worker_thread(i);
        });

    // Allow all worker threads to start
    start_barrier_.arrive_and_wait();
}

template <Strategy strategy> ThreadPool<strategy>::~ThreadPool()
{
    stop_flag_.test_and_set(std::memory_order_relaxed);

    for (std::thread &worker : workers_) {
        if (worker.joinable())
            worker.join();
    }
}

template class ThreadPool<Strategy::WorkSharing>;
template class ThreadPool<Strategy::WorkStealing>;
