#ifndef WF_RPC_RPC_CONFIG_H
#define WF_RPC_RPC_CONFIG_H

#include <string>
#include <map>
#include <vector>

namespace wf_rpc
{

struct UpstreamServerConfig
{
    std::string address;
    unsigned short weight;
};

struct UpstreamConfig
{
    std::string name;
    std::string type;
    bool try_another;
    std::vector<UpstreamServerConfig> servers;
};

struct CircuitBreakerConfig
{
    std::string service_name;
    int failure_threshold;
    int success_threshold;
    int timeout_ms;
};

struct RateLimiterConfig
{
    std::string service_name;
    int qps;
};

struct ServerConfig
{
    std::string address;
    std::string protocol;
    int iothread_num;
    int port;
};

struct RpcConfig
{
    ServerConfig server;
    std::vector<UpstreamConfig> upstreams;
    std::vector<CircuitBreakerConfig> circuit_breakers;
    std::vector<RateLimiterConfig> rate_limiters;
    std::string log_level;
    std::string log_path;
};

int InitConfig(const std::string& config_file);
const RpcConfig& GetConfig();
int StartRpcServer();

// 创建基于配置文件的TinyPB RPC客户端通道
// 如果配置文件中有upstream配置，则使用第一个upstream
// 否则使用server配置中的地址
std::string GetClientUpstreamUrl();

}

#endif