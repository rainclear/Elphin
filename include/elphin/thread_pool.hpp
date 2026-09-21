#ifndef ELPHIN_THREAD_POOL_HPP
#define ELPHIN_THREAD_POOL_HPP

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

namespace elphin::concurrent {

class ThreadPool {
public:
    explicit ThreadPool(size_t threads = std::thread::hardware_concurrency()) {
        workers_.reserve(threads);
        for (size_t i = 0; i < threads; ++i) {
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
        
        // Request stop across all std::jthreads
        for (auto& worker : workers_) {
            worker.request_stop();
        }
        
        cv_.notify_all();
    }

    // Enqueue a task for execution using std::invoke_result_t (C++17/C++20)
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

                task = std::move(tasks_.front());
                tasks_.pop();
            }

            task();
        }
    }

    std::vector<std::jthread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    bool stopping_{false};
};

} // namespace elphin::concurrent

#endif // ELPHIN_THREAD_POOL_HPP