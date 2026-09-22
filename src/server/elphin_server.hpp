#ifndef ELPHIN_SERVER_ELPHIN_SERVER_HPP
#define ELPHIN_SERVER_ELPHIN_SERVER_HPP

#include <unordered_map>
#include <memory>
#include <string>

#include "server/config.hpp"
#include "net/reactor.hpp"
#include "net/acceptor.hpp"
#include "net/connection.hpp"
#include "storage/db.hpp"
#include "common/thread_pool.hpp"
#include "protocol/resp_parser.hpp"

namespace elphin::server {

class ElphinServer {
public:
    explicit ElphinServer(const ServerConfig& config = ServerConfig{});
    ~ElphinServer();

    // 禁用拷贝与移动
    ElphinServer(const ElphinServer&) = delete;
    ElphinServer& operator=(const ElphinServer&) = delete;
    ElphinServer(ElphinServer&&) = delete;
    ElphinServer& operator=(ElphinServer&&) = delete;

    // 启动与停止服务
    void start();
    void stop();

private:
    void on_new_connection(int sockfd);
    void on_message(const net::ConnectionPtr& conn, net::Buffer* buf);
    void on_close(const net::ConnectionPtr& conn);

    // RESP 命令路由与处理
    void dispatch_command(const net::ConnectionPtr& conn, const resp::Command& cmd);

    ServerConfig config_;
    net::Reactor reactor_;
    net::Acceptor acceptor_;
    
    storage::Database db_;
    common::ThreadPool thread_pool_;

    std::unordered_map<int, net::ConnectionPtr> connections_;
};

} // namespace elphin::server

#endif // ELPHIN_SERVER_ELPHIN_SERVER_HPP