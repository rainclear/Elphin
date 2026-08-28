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

            // Handle incoming data via RESP Parser
            conn->set_message_callback([](const elphin::net::ConnectionPtr& c, elphin::net::Buffer* buf) {
                while (true) {
                    elphin::resp::Command cmd;
                    auto status = elphin::resp::RespParser::parse_command(buf, cmd);

                    if (status == elphin::resp::ParseStatus::Incomplete) {
                        break; // Wait for more data from socket
                    }

                    if (status == elphin::resp::ParseStatus::Error) {
                        c->send(elphin::resp::make_error("ERR Protocol error"));
                        buf->retrieve_all();
                        break;
                    }

                    if (cmd.args.empty()) continue;

                    // Convert command name to uppercase for case-insensitive matching
                    std::string cmd_name = cmd.args[0];
                    std::transform(cmd_name.begin(), cmd_name.end(), cmd_name.begin(), ::toupper);

                    if (cmd_name == "PING") {
                        if (cmd.args.size() > 1) {
                            c->send(elphin::resp::make_bulk_string(cmd.args[1]));
                        } else {
                            c->send(elphin::resp::make_simple_string("PONG"));
                        }
                    } else if (cmd_name == "ECHO") {
                        if (cmd.args.size() > 1) {
                            c->send(elphin::resp::make_bulk_string(cmd.args[1]));
                        } else {
                            c->send(elphin::resp::make_error("ERR wrong number of arguments for 'echo' command"));
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