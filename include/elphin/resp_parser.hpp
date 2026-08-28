#ifndef ELPHIN_RESP_PARSER_HPP
#define ELPHIN_RESP_PARSER_HPP

#include <string>
#include <vector>
#include <optional>
#include <string_view>
#include "elphin/buffer.hpp"

namespace elphin::resp {

enum class ParseStatus {
    Success,
    Incomplete,
    Error
};

struct Command {
    std::vector<std::string> args;
};

class RespParser {
public:
    // Parses a single RESP Array command from the read buffer.
    // Returns ParseStatus::Success and populates 'cmd' when a full command is ready.
    // Returns ParseStatus::Incomplete if more network data is needed.
    static ParseStatus parse_command(net::Buffer* buf, Command& cmd);
};

// Response Formatting Helpers
inline std::string make_simple_string(std::string_view str) {
    return "+" + std::string(str) + "\r\n";
}

inline std::string make_error(std::string_view err) {
    return "-" + std::string(err) + "\r\n";
}

inline std::string make_bulk_string(std::string_view str) {
    return "$" + std::to_string(str.size()) + "\r\n" + std::string(str) + "\r\n";
}

inline std::string make_null_bulk_string() {
    return "$-1\r\n";
}

} // namespace elphin::resp

#endif // ELPHIN_RESP_PARSER_HPP
