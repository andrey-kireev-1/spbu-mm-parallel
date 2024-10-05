#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <deque>
#include <optional>

template <typename T> class ThreadSafeDeque {
private:
    std::mutex mtx;
    std::deque<T> deque;

public:
    void unsafe_enqueue(T &&value)
    {
        deque.emplace_back(std::forward<T>(value));
    }

    std::optional<T> unsafe_dequeue()
    {
        if (!deque.empty()) {
            T value = std::move(deque.front());
            deque.pop_front();
            return std::optional<T>(std::move(value));
        }
        return {};
    }

    size_t unsafe_size()
    {
        return deque.size();
    }

    void enqueue(T &&value)
    {
        std::lock_guard<std::mutex> lock(mtx);
        return unsafe_enqueue(std::move(value));
    }

    std::optional<T> dequeue()
    {
        std::lock_guard<std::mutex> lock(mtx);
        return unsafe_dequeue();
    }

    size_t size()
    {
        std::lock_guard<std::mutex> lock(mtx);
        return unsafe_size();
    }

    std::optional<T> dequeue_top()
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (!deque.empty()) {
            T value = std::move(deque.front());
            deque.pop_front();
            return std::optional<T>(std::move(value));
        }
        return {};
    }

    std::mutex &get_mutex() { return mtx; }
};
