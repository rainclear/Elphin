#include "protocol/resp_parser.hpp"
#include <charconv>
#include <system_error>

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

// 辅助函数：安全将 string_view 解析为整数
static bool parse_int(std::string_view sv, int& out_val) {
    if (sv.empty()) return false;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), out_val);
    return ec == std::errc{} && ptr == sv.data() + sv.size();
}

ParseStatus RespParser::parse_command(net::Buffer* buf, Command& cmd) {
    if (buf->readable_bytes() == 0) {
        return ParseStatus::Incomplete;
    }

    const char* data = buf->peek();
    size_t total_readable = buf->readable_bytes();

    // 1. 处理 Inline 命令 (如 plain "PING\r\n")
    if (data[0] != '*') {
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

    // 2. 解析 RESP Array Header: *<number-of-elements>\r\n
    size_t line_consumed = 0;
    auto line = read_line(data, total_readable, line_consumed);
    if (!line.has_value()) {
        return ParseStatus::Incomplete;
    }

    int num_args = 0;
    if (!parse_int(line.value().substr(1), num_args) || num_args <= 0) {
        buf->retrieve(line_consumed);
        return ParseStatus::Error;
    }

    size_t current_offset = line_consumed;
    std::vector<std::string> parsed_args;
    parsed_args.reserve(num_args);

    // 3. 解析各个 Bulk String 元素: $<length>\r\n<data>\r\n
    for (int i = 0; i < num_args; ++i) {
        if (current_offset >= total_readable) {
            return ParseStatus::Incomplete;
        }

        size_t elem_line_consumed = 0;
        auto elem_line = read_line(data + current_offset, total_readable - current_offset, elem_line_consumed);
        if (!elem_line.has_value()) {
            return ParseStatus::Incomplete;
        }

        std::string_view sv = elem_line.value();
        if (sv.empty() || sv[0] != '$') {
            return ParseStatus::Error;
        }

        int str_len = 0;
        if (!parse_int(sv.substr(1), str_len) || str_len < 0) {
            return ParseStatus::Error;
        }

        current_offset += elem_line_consumed;

        // 检查完整的 Bulk String 载荷 + \r\n 是否已到达
        if (total_readable - current_offset < static_cast<size_t>(str_len + 2)) {
            return ParseStatus::Incomplete;
        }

        parsed_args.emplace_back(data + current_offset, str_len);
        current_offset += str_len + 2; // 跳过 payload 与 \r\n
    }

    // 解析成功，消费 Buffer
    buf->retrieve(current_offset);
    cmd.args = std::move(parsed_args);
    return ParseStatus::Success;
}

} // namespace elphin::resp