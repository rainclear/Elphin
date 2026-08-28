#include "elphin/resp_parser.hpp"
#include <cstring>
#include <cstdlib>

namespace elphin::resp {

static std::optional<std::string_view> read_line(const char* data, size_t len, size_t& bytes_consumed) {
    for (size_t i = 0; i < len - 1; ++i) {
        if (data[i] == '\r' && data[i + 1] == '\n') {
            bytes_consumed = i + 2;
            return std::string_view(data, i);
        }
    }
    return std::nullopt;
}

ParseStatus RespParser::parse_command(net::Buffer* buf, Command& cmd) {
    if (buf->readable_bytes() == 0) {
        return ParseStatus::Incomplete;
    }

    const char* data = buf->peek();
    size_t total_readable = buf->readable_bytes();

    // Redis commands are formatted as RESP Arrays starting with '*'
    if (data[0] != '*') {
        // Fallback for simple inline commands (e.g. plain "PING\r\n" via netcat)
        size_t consumed = 0;
        auto line = read_line(data, total_readable, consumed);
        if (!line.has_value()) {
            return ParseStatus::Incomplete;
        }

        std::string_view sv = line.value();
        std::string token;
        for (char ch : sv) {
            if (ch == ' ') {
                if (!token.empty()) {
                    cmd.args.push_back(token);
                    token.clear();
                }
            } else {
                token += ch;
            }
        }
        if (!token.empty()) {
            cmd.args.push_back(token);
        }

        buf->retrieve(consumed);
        return ParseStatus::Success;
    }

    // Parse RESP Array: *<number-of-elements>\r\n
    size_t line_consumed = 0;
    auto line = read_line(data, total_readable, line_consumed);
    if (!line.has_value()) {
        return ParseStatus::Incomplete;
    }

    int num_args = std::atoi(line.value().data() + 1);
    if (num_args <= 0) {
        buf->retrieve(line_consumed);
        return ParseStatus::Error;
    }

    size_t current_offset = line_consumed;
    std::vector<std::string> parsed_args;
    parsed_args.reserve(num_args);

    // Parse each Bulk String element: $<length>\r\n<data>\r\n
    for (int i = 0; i < num_args; ++i) {
        if (current_offset >= total_readable) {
            return ParseStatus::Incomplete;
        }

        size_t elem_line_consumed = 0;
        auto elem_line = read_line(data + current_offset, total_readable - current_offset, elem_line_consumed);
        if (!elem_line.has_value()) {
            return ParseStatus::Incomplete;
        }

        if (elem_line.value()[0] != '$') {
            return ParseStatus::Error;
        }

        int str_len = std::atoi(elem_line.value().data() + 1);
        current_offset += elem_line_consumed;

        // Check if full bulk string data + \r\n is available
        if (total_readable - current_offset < static_cast<size_t>(str_len + 2)) {
            return ParseStatus::Incomplete;
        }

        parsed_args.emplace_back(data + current_offset, str_len);
        current_offset += str_len + 2; // Move past payload and \r\n
    }

    // Successfully parsed a full command; advance read index in Buffer
    buf->retrieve(current_offset);
    cmd.args = std::move(parsed_args);
    return ParseStatus::Success;
}

} // namespace elphin::resp
