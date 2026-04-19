#pragma once
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(unsigned num_threads);
    ~ThreadPool();

    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

private:
    std::vector<std::thread>        workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex                      mutex_;
    std::condition_variable         cv_;
    bool                            stop_ = false;
};

template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>>
{
    using R = std::invoke_result_t<F, Args...>;
    auto task = std::make_shared<std::packaged_task<R()>>(
        [f = std::forward<F>(f),
         tup = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            return std::apply(std::move(f), std::move(tup));
        });
    auto fut = task->get_future();
    {
        std::lock_guard lock(mutex_);
        if (stop_) throw std::runtime_error("ThreadPool is stopped");
        tasks_.emplace([task = std::move(task)] { (*task)(); });
    }
    cv_.notify_one();
    return fut;
}
