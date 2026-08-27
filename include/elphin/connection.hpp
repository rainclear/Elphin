#ifndef ELPHIN_CONNECTION_HPP
#define ELPHIN_CONNECTION_HPP

#include <memory>
#include <functional>
#include "elphin/buffer.hpp"
#include "elphin/reactor.hpp"

namespace elphin::net {

class Connection;
using ConnectionPtr = std::shared_ptr<Connection>;
using MessageCallback = std::function<void(const ConnectionPtr&, Buffer*)>;
using CloseCallback = std::function<void(const ConnectionPtr&)>;

class Connection : public std::enable_shared_from_this<Connection> {
public:
    Connection(Reactor* reactor, int fd);
    ~Connection();

    int fd() const { return fd_; }
    
    void set_message_callback(MessageCallback cb) { message_callback_ = std::move(cb); }
    void set_close_callback(CloseCallback cb) { close_callback_ = std::move(cb); }

    void establish_connection();
    void send(std::string_view msg);
    void handle_close();

private:
    void handle_read();
    void handle_write();

    Reactor* reactor_;
    int fd_;
    Buffer read_buffer_;
    Buffer write_buffer_;

    MessageCallback message_callback_;
    CloseCallback close_callback_;
};

} // namespace elphin::net

#endif // ELPHIN_CONNECTION_HPP