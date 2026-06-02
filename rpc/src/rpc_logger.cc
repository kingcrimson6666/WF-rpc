#include "rpc_logger.h"
#include <cstdarg>
#include <cstdio>
#include <iomanip>

namespace wf_rpc
{

RpcLogger::RpcLogger() 
    : level_(LOG_LEVEL_INFO), console_output_(true)
{
}

RpcLogger::~RpcLogger()
{
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

RpcLogger& RpcLogger::instance()
{
    static RpcLogger instance;
    return instance;
}

void RpcLogger::set_level(LogLevel level)
{
    level_ = level;
}

void RpcLogger::set_output_file(const std::string& filename)
{
    std::lock_guard<std::mutex> lock(mutex_);
    log_file_ = filename;
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
    file_stream_.open(filename, std::ios::app);
}

void RpcLogger::enable_console_output(bool enable)
{
    console_output_ = enable;
}

void RpcLogger::debug(const std::string& message)
{
    if (level_ <= LOG_LEVEL_DEBUG) {
        log(LOG_LEVEL_DEBUG, message);
    }
}

void RpcLogger::info(const std::string& message)
{
    if (level_ <= LOG_LEVEL_INFO) {
        log(LOG_LEVEL_INFO, message);
    }
}

void RpcLogger::warn(const std::string& message)
{
    if (level_ <= LOG_LEVEL_WARN) {
        log(LOG_LEVEL_WARN, message);
    }
}

void RpcLogger::error(const std::string& message)
{
    if (level_ <= LOG_LEVEL_ERROR) {
        log(LOG_LEVEL_ERROR, message);
    }
}

void RpcLogger::fatal(const std::string& message)
{
    if (level_ <= LOG_LEVEL_FATAL) {
        log(LOG_LEVEL_FATAL, message);
    }
}

void RpcLogger::debugf(const char* format, ...)
{
    if (level_ > LOG_LEVEL_DEBUG) return;
    
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(LOG_LEVEL_DEBUG, buffer);
}

void RpcLogger::infof(const char* format, ...)
{
    if (level_ > LOG_LEVEL_INFO) return;
    
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(LOG_LEVEL_INFO, buffer);
}

void RpcLogger::warnf(const char* format, ...)
{
    if (level_ > LOG_LEVEL_WARN) return;
    
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(LOG_LEVEL_WARN, buffer);
}

void RpcLogger::errorf(const char* format, ...)
{
    if (level_ > LOG_LEVEL_ERROR) return;
    
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(LOG_LEVEL_ERROR, buffer);
}

void RpcLogger::fatalf(const char* format, ...)
{
    if (level_ > LOG_LEVEL_FATAL) return;
    
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(LOG_LEVEL_FATAL, buffer);
}

void RpcLogger::log(LogLevel level, const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string formatted = format_message(level, message);
    
    if (console_output_) {
        std::fprintf(stdout, "%s\n", formatted.c_str());
        std::fflush(stdout);
    }
    
    if (file_stream_.is_open()) {
        file_stream_ << formatted << "\n";
        file_stream_.flush();
    }
}

std::string RpcLogger::format_message(LogLevel level, const std::string& message)
{
    std::ostringstream oss;
    oss << get_timestamp() << " [" << level_to_string(level) << "] " << message;
    return oss.str();
}

std::string RpcLogger::get_timestamp()
{
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

const char* RpcLogger::level_to_string(LogLevel level)
{
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_FATAL: return "FATAL";
        default:              return "UNKNOWN";
    }
}

}