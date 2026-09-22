#ifndef ELPHIN_COMMON_LOGGER_HPP
#define ELPHIN_COMMON_LOGGER_HPP

#include <iostream>
#include <mutex>
#include <string_view>
#include <chrono>
#include <format>

namespace elphin::common {

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    static Logger& instance() {
        static Logger instance;
        return instance;
    }

    void set_level(LogLevel level) {
        current_level_ = level;
    }

    template<typename... Args>
    void log(LogLevel level, std::string_view fmt, Args&&... args) {
        if (level < current_level_) return;

        auto now = std::chrono::system_clock::now();
        std::string message = std::vformat(fmt, std::make_format_args(args...));

        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << std::format("[{}] [{}] {}\n", 
                                 now, 
                                 level_to_string(level), 
                                 message);
    }

private:
    Logger() = default;

    constexpr const char* level_to_string(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERROR: return "ERROR";
        }
        return "UNKNOWN";
    }

    LogLevel current_level_{LogLevel::INFO};
    std::mutex mutex_;
};

} // namespace elphin::common

#define LOG_DEBUG(fmt, ...) ::elphin::common::Logger::instance().log(::elphin::common::LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  ::elphin::common::Logger::instance().log(::elphin::common::LogLevel::INFO,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  ::elphin::common::Logger::instance().log(::elphin::common::LogLevel::WARN,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) ::elphin::common::Logger::instance().log(::elphin::common::LogLevel::ERROR, fmt, ##__VA_ARGS__)

#endif // ELPHIN_COMMON_LOGGER_HPP