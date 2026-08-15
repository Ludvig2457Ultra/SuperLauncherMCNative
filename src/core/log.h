#pragma once
#include <string>

namespace sl {
enum LogLevel { LOG_INFO, LOG_WARN, LOG_ERROR, LOG_DEBUG };
void log_set_file(const std::string& path);
void log_msg(LogLevel lvl, const std::string& msg);
void log_info(const std::string& msg);
void log_warn(const std::string& msg);
void log_error(const std::string& msg);
void log_debug(const std::string& msg);
} // namespace sl