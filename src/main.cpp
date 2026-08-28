#include <iostream>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <cerrno>
#include "elphin/common.hpp"
#include "elphin/socket_utils.hpp"
#include "elphin/reactor.hpp"
#include "elphin/connection.hpp"
#include "elphin/resp_parser.hpp"
#include "elphin/db.hpp"

int main() {
    std::cout << "Starting " << elphin::PROJECT_NAME << " v" << elphin::VERSION << "...\n";

    constexpr uint16_t PORT = 6379;
    int listen_fd = elphin::net::create_server_socket(PORT);
    if (listen_fd < 0) {
        std::cerr << "Failed to bind listener on port " << PORT << "\n";
        return 1;
    }

    std::cout << "[Elphin Core] Listening on port " << PORT << "...\n";

    elphin::net::Reactor reactor;
    std::unordered_map<int, elphin::net::ConnectionPtr> connections;
    
    // Instantiate our Key-Value Store
    elphin::store::Database db;

    reactor.add_fd(listen_fd, EPOLLIN, [&](uint32_t) {
        while (true) {
            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            int client_fd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

            if (client_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                if (errno == EINTR) continue;
                break;
            }

            elphin::net::set_non_blocking(client_fd);
            elphin::net::set_tcp_nodelay(client_fd);

            auto conn = std::make_shared<elphin::net::Connection>(&reactor, client_fd);
            connections[client_fd] = conn;

            conn->set_message_callback([&db](const elphin::net::ConnectionPtr& c, elphin::net::Buffer* buf) {
                while (true) {
                    elphin::resp::Command cmd;
                    auto status = elphin::resp::RespParser::parse_command(buf, cmd);

                    if (status == elphin::resp::ParseStatus::Incomplete) {
                        break;
                    }

                    if (status == elphin::resp::ParseStatus::Error) {
                        c->send(elphin::resp::make_error("ERR Protocol error"));
                        buf->retrieve_all();
                        break;
                    }

                    if (cmd.args.empty()) continue;

                    std::string cmd_name = cmd.args[0];
                    std::transform(cmd_name.begin(), cmd_name.end(), cmd_name.begin(), ::toupper);

                    // Command Dispatching
                    if (cmd_name == "PING") {
                        if (cmd.args.size() > 1) {
                            c->send(elphin::resp::make_bulk_string(cmd.args[1]));
                        } else {
                            c->send(elphin::resp::make_simple_string("PONG"));
                        }
                    } else if (cmd_name == "SET") {
                        if (cmd.args.size() >= 3) {
                            db.set(cmd.args[1], cmd.args[2]);
                            c->send(elphin::resp::make_simple_string("OK"));
                        } else {
                            c->send(elphin::resp::make_error("ERR wrong number of arguments for 'set' command"));
                        }
                    } else if (cmd_name == "GET") {
                        if (cmd.args.size() == 2) {
                            auto val = db.get(cmd.args[1]);
                            if (val.has_value()) {
                                c->send(elphin::resp::make_bulk_string(val.value()));
                            } else {
                                c->send(elphin::resp::make_null_bulk_string());
                            }
                        } else {
                            c->send(elphin::resp::make_error("ERR wrong number of arguments for 'get' command"));
                        }
                    } else if (cmd_name == "DEL") {
                        if (cmd.args.size() == 2) {
                            bool deleted = db.del(cmd.args[1]);
                            c->send(":" + std::to_string(deleted ? 1 : 0) + "\r\n");
                        } else {
                            c->send(elphin::resp::make_error("ERR wrong number of arguments for 'del' command"));
                        }
                    } else if (cmd_name == "EXISTS") {
                        if (cmd.args.size() == 2) {
                            bool exists = db.exists(cmd.args[1]);
                            c->send(":" + std::to_string(exists ? 1 : 0) + "\r\n");
                        } else {
                            c->send(elphin::resp::make_error("ERR wrong number of arguments for 'exists' command"));
                        }
                    } else if (cmd_name == "ZADD") {
                        if (cmd.args.size() == 4) {
                            try {
                                double score = std::stod(cmd.args[2]);
                                bool added = db.zadd(cmd.args[1], score, cmd.args[3]);
                                c->send(":" + std::to_string(added ? 1 : 0) + "\r\n");
                            } catch (...) {
                                c->send(elphin::resp::make_error("ERR value is not a valid float"));
                            }
                        } else {
                            c->send(elphin::resp::make_error("ERR wrong number of arguments for 'zadd' command"));
                        }
                    } else if (cmd_name == "ZRANGEBYSCORE") {
                        if (cmd.args.size() == 4) {
                            try {
                                double min_score = std::stod(cmd.args[2]);
                                double max_score = std::stod(cmd.args[3]);
                                auto range = db.zrangebyscore(cmd.args[1], min_score, max_score);

                                std::string resp = "*" + std::to_string(range.size()) + "\r\n";
                                for (const auto& [member, score] : range) {
                                    resp += elphin::resp::make_bulk_string(member);
                                }
                                c->send(resp);
                            } catch (...) {
                                c->send(elphin::resp::make_error("ERR min or max is not a float"));
                            }
                        } else {
                            c->send(elphin::resp::make_error("ERR wrong number of arguments for 'zrangebyscore' command"));
                        }
                    } else {
                        c->send(elphin::resp::make_error("ERR unknown command '" + cmd.args[0] + "'"));
                    }
                }
            });

            conn->set_close_callback([&connections](const elphin::net::ConnectionPtr& c) {
                std::cout << "[Elphin] Client disconnected fd: " << c->fd() << "\n";
                connections.erase(c->fd());
            });

            conn->establish_connection();
        }
    });

    while (true) {
        reactor.loop_once(100);
    }

    ::close(listen_fd);
    return 0;
}