#include "server/elphin_server.hpp"
#include "protocol/resp_builder.hpp"
#include "common/logger.hpp"
#include <algorithm>
#include <charconv>
#include <system_error>

namespace elphin::server {

ElphinServer::ElphinServer(const ServerConfig& config)
    : config_(config),
      acceptor_(&reactor_, config.port),
      thread_pool_(config.worker_threads) {

    acceptor_.set_new_connection_callback([this](int sockfd) {
        on_new_connection(sockfd);
    });
}

ElphinServer::~ElphinServer() {
    stop();
}

void ElphinServer::start() {
    if (!acceptor_.is_listening()) {
        LOG_ERROR("Failed to start server on port {}", config_.port);
        return;
    }

    LOG_INFO("Elphin Server listening on port {}", config_.port);
    acceptor_.listen();

    reactor_.run();
}

void ElphinServer::stop() {
    reactor_.stop();
}

void ElphinServer::on_new_connection(int sockfd) {
    auto conn = std::make_shared<net::Connection>(&reactor_, sockfd);

    conn->set_message_callback([this](const net::ConnectionPtr& c, net::Buffer* buf) {
        on_message(c, buf);
    });

    conn->set_close_callback([this](const net::ConnectionPtr& c) {
        on_close(c);
    });

    if (conn->establish_connection()) {
        connections_[sockfd] = conn;
        LOG_INFO("Client connected, fd={}", sockfd);
    } else {
        LOG_ERROR("Failed to register epoll event for client fd={}", sockfd);
    }
}

void ElphinServer::on_message(const net::ConnectionPtr& conn, net::Buffer* buf) {
    while (true) {
        resp::Command cmd;
        auto status = resp::RespParser::parse_command(buf, cmd);

        if (status == resp::ParseStatus::Incomplete) {
            break;
        }

        if (status == resp::ParseStatus::Error) {
            conn->send(resp::RespBuilder::make_error("ERR Protocol error"));
            buf->retrieve_all();
            break;
        }

        if (!cmd.args.empty()) {
            dispatch_command(conn, cmd);
        }
    }
}

void ElphinServer::on_close(const net::ConnectionPtr& conn) {
    LOG_INFO("Client disconnected, fd={}", conn->fd());
    connections_.erase(conn->fd());
}

void ElphinServer::dispatch_command(const net::ConnectionPtr& conn, const resp::Command& cmd) {
    std::string cmd_name = cmd.args[0];
    std::transform(cmd_name.begin(), cmd_name.end(), cmd_name.begin(), ::toupper);

    if (cmd_name == "PING") {
        if (cmd.args.size() > 1) {
            conn->send(resp::RespBuilder::make_bulk_string(cmd.args[1]));
        } else {
            conn->send(resp::RespBuilder::make_simple_string("PONG"));
        }
    } else if (cmd_name == "SET") {
        if (cmd.args.size() >= 3) {
            db_.set(cmd.args[1], cmd.args[2]);
            conn->send(resp::RespBuilder::make_simple_string("OK"));
        } else {
            conn->send(resp::RespBuilder::make_error("ERR wrong number of arguments for 'set' command"));
        }
    } else if (cmd_name == "GET") {
        if (cmd.args.size() == 2) {
            auto val = db_.get(cmd.args[1]);
            if (val.has_value()) {
                conn->send(resp::RespBuilder::make_bulk_string(val.value()));
            } else {
                conn->send(resp::RespBuilder::make_null_bulk_string());
            }
        } else {
            conn->send(resp::RespBuilder::make_error("ERR wrong number of arguments for 'get' command"));
        }
    } else if (cmd_name == "DEL") {
        if (cmd.args.size() == 2) {
            bool deleted = db_.del(cmd.args[1]);
            conn->send(resp::RespBuilder::make_integer(deleted ? 1 : 0));
        } else {
            conn->send(resp::RespBuilder::make_error("ERR wrong number of arguments for 'del' command"));
        }
    } else if (cmd_name == "EXISTS") {
        if (cmd.args.size() == 2) {
            bool exists = db_.exists(cmd.args[1]);
            conn->send(resp::RespBuilder::make_integer(exists ? 1 : 0));
        } else {
            conn->send(resp::RespBuilder::make_error("ERR wrong number of arguments for 'exists' command"));
        }
    } else if (cmd_name == "ZADD") {
        if (cmd.args.size() == 4) {
            double score = 0.0;
            auto [ptr, ec] = std::from_chars(cmd.args[2].data(), cmd.args[2].data() + cmd.args[2].size(), score);
            if (ec != std::errc{} || ptr != cmd.args[2].data() + cmd.args[2].size()) {
                conn->send(resp::RespBuilder::make_error("ERR value is not a valid float"));
            } else {
                bool added = db_.zadd(cmd.args[1], score, cmd.args[3]);
                conn->send(resp::RespBuilder::make_integer(added ? 1 : 0));
            }
        } else {
            conn->send(resp::RespBuilder::make_error("ERR wrong number of arguments for 'zadd' command"));
        }
    } else if (cmd_name == "ZRANGEBYSCORE") {
        if (cmd.args.size() == 4) {
            double min_score = 0.0;
            double max_score = 0.0;
            auto [ptr1, ec1] = std::from_chars(cmd.args[2].data(), cmd.args[2].data() + cmd.args[2].size(), min_score);
            auto [ptr2, ec2] = std::from_chars(cmd.args[3].data(), cmd.args[3].data() + cmd.args[3].size(), max_score);

            if (ec1 != std::errc{} || ptr1 != cmd.args[2].data() + cmd.args[2].size() ||
                ec2 != std::errc{} || ptr2 != cmd.args[3].data() + cmd.args[3].size()) {
                conn->send(resp::RespBuilder::make_error("ERR min or max is not a float"));
            } else {
                auto range = db_.zrangebyscore(cmd.args[1], min_score, max_score);
                std::string resp = resp::RespBuilder::make_array_header(range.size());
                for (const auto& [member, score] : range) {
                    resp += resp::RespBuilder::make_bulk_string(member);
                }
                conn->send(resp);
            }
        } else {
            conn->send(resp::RespBuilder::make_error("ERR wrong number of arguments for 'zrangebyscore' command"));
        }
    } else if (cmd_name == "EXPIRE") {
        if (cmd.args.size() == 3) {
            int64_t seconds = 0;
            auto [ptr, ec] = std::from_chars(cmd.args[2].data(), cmd.args[2].data() + cmd.args[2].size(), seconds);
            if (ec != std::errc{} || ptr != cmd.args[2].data() + cmd.args[2].size()) {
                conn->send(resp::RespBuilder::make_error("ERR value is not an integer or out of range"));
            } else {
                bool set_exp = db_.expire(cmd.args[1], seconds);
                conn->send(resp::RespBuilder::make_integer(set_exp ? 1 : 0));
            }
        } else {
            conn->send(resp::RespBuilder::make_error("ERR wrong number of arguments for 'expire' command"));
        }
    } else if (cmd_name == "TTL") {
        if (cmd.args.size() == 2) {
            int64_t remain = db_.ttl(cmd.args[1]);
            conn->send(resp::RespBuilder::make_integer(remain));
        } else {
            conn->send(resp::RespBuilder::make_error("ERR wrong number of arguments for 'ttl' command"));
        }
    } else {
        conn->send(resp::RespBuilder::make_error("ERR unknown command '" + cmd.args[0] + "'"));
    }

    thread_pool_.enqueue([this]() {
        db_.active_expire_cycle(config_.active_expire_sample_size);
    });
}

} // namespace elphin::server