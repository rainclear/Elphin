#ifndef ELPHIN_SERVER_CONFIG_HPP
#define ELPHIN_SERVER_CONFIG_HPP

#include <cstdint>
#include <string>

namespace elphin::server {

struct ServerConfig {
    uint16_t port{6379};                  // 默认 Redis 监听端口
    size_t worker_threads{2};            // 后台清理/异步任务线程池大小
    int epoll_timeout_ms{100};           // Reactor loop_once 超时时间 (ms)
    size_t active_expire_sample_size{20};// 每次主动清理过期的采样 Key 数量
};

} // namespace elphin::server

#endif // ELPHIN_SERVER_CONFIG_HPP