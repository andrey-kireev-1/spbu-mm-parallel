#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <iostream>
#include <random>
#include <thread>
#include <vector>
#include <barrier>

#include "ThreadSafeDeque.h"

enum class Strategy { WorkSharing, WorkStealing };

template <Strategy strategy> class ThreadPool {
private:
    std::vector<std::thread> workers_;
    std::atomic<size_t> workers_size_{0};
    std::vector<ThreadSafeDeque<std::function<void()>>> tasks_;
    std::atomic_flag stop_flag_ = ATOMIC_FLAG_INIT;
    std::mutex queue_mutex_;
    std::mt19937 rng{std::random_device{}()};
    std::mutex rng_mutex;
    std::barrier<> start_barrier_; // Barrier to synchronize the start of the threads

    const size_t threshold_ = 2;

    void balance_queues(size_t index, size_t victim);

    void worker_thread(size_t index);

    size_t get_rng_index()
    {
        std::lock_guard<std::mutex> lock(rng_mutex);
        size_t max = workers_size_.load(std::memory_order_relaxed) - 1;

        return std::uniform_int_distribution<size_t>{0, max}(rng);
    }

public:
    explicit ThreadPool(size_t threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    template <typename F> auto enqueue(F &&f)
    {
        if (stop_flag_.test(std::memory_order_relaxed))
            throw std::runtime_error("enqueue on stopped ThreadPool");

        using return_type = typename std::result_of<F()>::type;
        auto task_ptr = std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(f));
        std::future<return_type> res = task_ptr->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            tasks_[get_rng_index()].enqueue([task_ptr] { (*task_ptr)(); });
        }

        return res;
    }

    template <typename T, typename Cont>
    auto continue_with(std::future<T> &&future, Cont &&continuation)
    {
        using cont_return_type = std::invoke_result_t<Cont, T>;

        auto task_wrapper = [fut = std::move(future),
                             cont = std::forward<Cont>(continuation)]() mutable {
            T result = fut.get();
            return cont(result);
        };

        // Due to the nature of std::packaged_task, which requires a callable of
        // signature void(), we need to match this requirement if the
        // continuation returns void. This adaptation is necessary because the
        // implementation detail of enqueue expects a no-argument callable.
        using task_return_type =
            std::conditional_t<std::is_same_v<cont_return_type, void>,
                               std::packaged_task<void()>,
                               std::packaged_task<cont_return_type()>>;

        auto packagedTask =
            std::make_shared<task_return_type>(std::move(task_wrapper));
        auto resultFuture = packagedTask->get_future();

        enqueue([packagedTask] { (*packagedTask)(); });

        return resultFuture;
    }
};
