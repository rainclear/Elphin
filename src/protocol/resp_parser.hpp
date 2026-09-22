#ifndef ELPHIN_PROTOCOL_RESP_PARSER_HPP
#define ELPHIN_PROTOCOL_RESP_PARSER_HPP

#include <string>
#include <vector>
#include <optional>
#include <string_view>
#include "net/buffer.hpp"

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
    // 从 Read Buffer 解析单条 RESP 命令
    static ParseStatus parse_command(net::Buffer* buf, Command& cmd);
};

} // namespace elphin::resp

#endif // ELPHIN_PROTOCOL_RESP_PARSER_HPP