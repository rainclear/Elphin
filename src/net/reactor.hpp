#ifndef ELPHIN_NET_REACTOR_HPP
#define ELPHIN_NET_REACTOR_HPP

#include <sys/epoll.h>
#include <functional>
#include <unordered_map>
#include <vector>

namespace elphin::net {

using EventCallback = std::function<void(uint32_t events)>;

class Reactor {
public:
    explicit Reactor(int max_events = 1024);
    ~Reactor();

    Reactor(const Reactor&) = delete;
    Reactor& operator=(const Reactor&) = delete;
    Reactor(Reactor&&) = delete;
    Reactor& operator=(Reactor&&) = delete;

    bool add_fd(int fd, uint32_t events, EventCallback callback);
    bool modify_fd(int fd, uint32_t events);
    bool remove_fd(int fd);

    // 运行一次事件循环
    void loop_once(int timeout_ms = -1);
    
    // 启动/停止事件循环
    void run();
    void stop();

    bool is_running() const { return running_; }

private:
    int epoll_fd_{-1};
    bool running_{false};
    std::vector<epoll_event> events_buffer_;
    std::unordered_map<int, EventCallback> callbacks_;
};

} // namespace elphin::net

#endif // ELPHIN_NET_REACTOR_HPP