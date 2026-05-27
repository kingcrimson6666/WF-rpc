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
    std::string log_level;
    std::string log_path;
};

int InitConfig(const std::string& config_file);
const RpcConfig& GetConfig();
int StartRpcServer();

}

#endif