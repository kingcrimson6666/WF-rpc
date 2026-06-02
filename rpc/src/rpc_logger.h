#ifndef WF_RPC_LOGGER_H
#define WF_RPC_LOGGER_H

#include <string>
#include <sstream>
#include <mutex>
#include <fstream>
#include <chrono>

namespace wf_rpc
{

enum LogLevel {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_FATAL = 4
};

class RpcLogger
{
public:
    static RpcLogger& instance();
    
    void set_level(LogLevel level);
    void set_output_file(const std::string& filename);
    void enable_console_output(bool enable);
    
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
    void fatal(const std::string& message);
    
    void debugf(const char* format, ...);
    void infof(const char* format, ...);
    void warnf(const char* format, ...);
    void errorf(const char* format, ...);
    void fatalf(const char* format, ...);

private:
    RpcLogger();
    ~RpcLogger();
    RpcLogger(const RpcLogger&) = delete;
    RpcLogger& operator=(const RpcLogger&) = delete;
    
    void log(LogLevel level, const std::string& message);
    std::string format_message(LogLevel level, const std::string& message);
    std::string get_timestamp();
    const char* level_to_string(LogLevel level);

private:
    std::mutex mutex_;
    std::ofstream file_stream_;
    std::string log_file_;
    LogLevel level_;
    bool console_output_;
};

#define RPC_LOG_DEBUG(msg) wf_rpc::RpcLogger::instance().debug(msg)
#define RPC_LOG_INFO(msg) wf_rpc::RpcLogger::instance().info(msg)
#define RPC_LOG_WARN(msg) wf_rpc::RpcLogger::instance().warn(msg)
#define RPC_LOG_ERROR(msg) wf_rpc::RpcLogger::instance().error(msg)
#define RPC_LOG_FATAL(msg) wf_rpc::RpcLogger::instance().fatal(msg)

#define RPC_LOG_DEBUGF(...) wf_rpc::RpcLogger::instance().debugf(__VA_ARGS__)
#define RPC_LOG_INFOF(...) wf_rpc::RpcLogger::instance().infof(__VA_ARGS__)
#define RPC_LOG_WARNF(...) wf_rpc::RpcLogger::instance().warnf(__VA_ARGS__)
#define RPC_LOG_ERRORF(...) wf_rpc::RpcLogger::instance().errorf(__VA_ARGS__)
#define RPC_LOG_FATALF(...) wf_rpc::RpcLogger::instance().fatalf(__VA_ARGS__)

}

#endif