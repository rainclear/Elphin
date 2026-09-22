#ifndef ELPHIN_PROTOCOL_RESP_BUILDER_HPP
#define ELPHIN_PROTOCOL_RESP_BUILDER_HPP

#include <string>
#include <string_view>
#include <vector>
#include <format>

namespace elphin::resp {

class RespBuilder {
public:
    // Simple String: +OK\r\n
    static std::string make_simple_string(std::string_view str) {
        return std::format("+{}\r\n", str);
    }

    // Error: -ERR message\r\n
    static std::string make_error(std::string_view err) {
        return std::format("-{}\r\n", err);
    }

    // Integer: :10\r\n
    static std::string make_integer(int64_t val) {
        return std::format(":{}\r\n", val);
    }

    // Bulk String: $5\r\nhello\r\n
    static std::string make_bulk_string(std::string_view str) {
        return std::format("${}\r\n{}\r\n", str.size(), str);
    }

    // Null Bulk String: $-1\r\n
    static std::string make_null_bulk_string() {
        return "$-1\r\n";
    }

    // Array Header: *3\r\n
    static std::string make_array_header(size_t size) {
        return std::format("*{}\r\n", size);
    }

    // Null Array: *-1\r\n
    static std::string make_null_array() {
        return "*-1\r\n";
    }
};

} // namespace elphin::resp

#endif // ELPHIN_PROTOCOL_RESP_BUILDER_HPP