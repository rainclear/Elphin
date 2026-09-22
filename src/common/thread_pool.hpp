#ifndef ELPHIN_COMMON_THREAD_POOL_HPP
#define ELPHIN_COMMON_THREAD_POOL_HPP

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <type_traits>
#include <utility>
#include <stdexcept>
#include <memory>
#include <stop_token>

namespace elphin::common {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t threads = std::thread::hardware_concurrency()) {
        // Fallback to 2 threads if hardware_concurrency returns 0[cite: 2]
        if (threads == 0) {
            threads = 2;
        }

        workers_.reserve(threads);
        for (std::size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this](std::stop_token stop_tok) {
                worker_loop(stop_tok);
            });
        }
    }

    // Non-copyable and non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    ~ThreadPool() {
        stop();
    }

    // Explicitly stop all workers and wake up waiting threads
    void stop() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (stopping_) {
                return;
            }
            stopping_ = true;
        }
        
        // 1. Explicitly send stop request to all jthreads[cite: 3]
        for (auto& worker : workers_) {
            worker.request_stop();
        }
        
        // 2. Wake up all threads blocked on cv_.wait[cite: 3]
        cv_.notify_all();
    }

    // Enqueue a task for execution using std::invoke_result_t (C++17/C++20)[cite: 3]
    template <typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<std::invoke_result_t<F, Args...>> {
        
        using return_type = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> res = task->get_future();
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);

            if (stopping_) {
                throw std::runtime_error("enqueue called on stopped ThreadPool");
            }

            tasks_.emplace([task]() { (*task)(); });
        }
        
        cv_.notify_one();
        return res;
    }

    // Thread pool status inspection methods[cite: 2]
    [[nodiscard]] std::size_t thread_count() const noexcept {
        return workers_.size();
    }

    [[nodiscard]] std::size_t pending_tasks() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }

private:
    void worker_loop(std::stop_token stop_tok) {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                cv_.wait(lock, [this, &stop_tok] {
                    return stopping_ || stop_tok.stop_requested() || !tasks_.empty();
                });

                if ((stopping_ || stop_tok.stop_requested()) && tasks_.empty()) {
                    return;
                }

                if (!tasks_.empty()) {
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
            }

            // Defensive check to ensure task is valid before execution[cite: 2]
            if (task) {
                task();
            }
        }
    }

    std::vector<std::jthread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex queue_mutex_; // Mutable allows locking inside const member functions[cite: 2]
    std::condition_variable cv_;
    bool stopping_{false};
};

} // namespace elphin::common

#endif // ELPHIN_COMMON_THREAD_POOL_HPP