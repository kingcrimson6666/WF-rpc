#include "rpc_config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>
#include <cctype>
#include <algorithm>
#include "tinypb_rpc_server.h"
#include "rpc_framework.h"
#include "rpc_logger.h"
#include "rpc_service_governance.h"

namespace wf_rpc
{

static RpcConfig g_config;

int parse_xml(const std::string& content, RpcConfig& config);

int InitConfig(const std::string& config_file)
{
    std::ifstream file(config_file);
    if (!file.is_open())
    {
        std::cerr << "Failed to open config file: " << config_file << std::endl;
        return -1;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    return parse_xml(content, g_config);
}

const RpcConfig& GetConfig()
{
    return g_config;
}

static void cleanup_upstreams(const std::vector<std::string>& created_upstreams)
{
    for (const auto& name : created_upstreams)
    {
        RpcClient::delete_upstream(name);
    }
}

int StartRpcServer()
{
    const RpcConfig& config = GetConfig();

    // 应用日志配置
    if (!config.log_level.empty())
    {
        LogLevel level = LOG_LEVEL_INFO;
        if (config.log_level == "DEBUG")
            level = LOG_LEVEL_DEBUG;
        else if (config.log_level == "INFO")
            level = LOG_LEVEL_INFO;
        else if (config.log_level == "WARN")
            level = LOG_LEVEL_WARN;
        else if (config.log_level == "ERROR")
            level = LOG_LEVEL_ERROR;
        else if (config.log_level == "FATAL")
            level = LOG_LEVEL_FATAL;
        
        RpcLogger::instance().set_level(level);
        std::cout << "Log level set to: " << config.log_level << std::endl;
    }

    if (!config.log_path.empty())
    {
        RpcLogger::instance().set_output_file(config.log_path);
        std::cout << "Log output file set to: " << config.log_path << std::endl;
    }

    // 应用服务治理配置
    for (const auto& cb : config.circuit_breakers)
    {
        ServiceGovernanceManager::instance().get_circuit_breaker(
            cb.service_name, cb.failure_threshold, cb.success_threshold, cb.timeout_ms);
        std::cout << "Circuit breaker configured for service: " << cb.service_name
                  << " (failure_threshold=" << cb.failure_threshold
                  << ", success_threshold=" << cb.success_threshold
                  << ", timeout_ms=" << cb.timeout_ms << ")" << std::endl;
    }

    for (const auto& rl : config.rate_limiters)
    {
        ServiceGovernanceManager::instance().get_rate_limiter(rl.service_name, rl.qps);
        std::cout << "Rate limiter configured for service: " << rl.service_name
                  << " (qps=" << rl.qps << ")" << std::endl;
    }

    std::string host;
    int port = 0;

    size_t colon_pos = config.server.address.find(':');
    if (colon_pos != std::string::npos)
    {
        host = config.server.address.substr(0, colon_pos);
        try
        {
            port = std::stoi(config.server.address.substr(colon_pos + 1));
        }
        catch (...)
        {
            std::cerr << "Invalid port in address: " << config.server.address << std::endl;
            return -1;
        }
    }
    else
    {
        host = config.server.address;
        port = config.server.port > 0 ? config.server.port : 20000;
    }

    std::vector<std::string> created_upstreams;

    for (const auto& upstream : config.upstreams)
    {
        if (upstream.type == "weighted_random")
        {
            if (RpcClient::create_weighted_upstream(upstream.name, upstream.try_another) != 0)
            {
                std::cerr << "Failed to create upstream: " << upstream.name << std::endl;
                cleanup_upstreams(created_upstreams);
                return -1;
            }
            created_upstreams.push_back(upstream.name);

            for (const auto& server : upstream.servers)
            {
                struct AddressParams params = ADDRESS_PARAMS_DEFAULT;
                params.weight = server.weight;
                if (RpcClient::add_upstream_server(upstream.name, server.address, &params) != 0)
                {
                    std::cerr << "Failed to add server: " << server.address << std::endl;
                    cleanup_upstreams(created_upstreams);
                    return -1;
                }
            }
        }
        else if (upstream.type == "consistent_hash")
        {
            if (RpcClient::create_consistent_hash_upstream(upstream.name, nullptr) != 0)
            {
                std::cerr << "Failed to create consistent hash upstream: " << upstream.name << std::endl;
                cleanup_upstreams(created_upstreams);
                return -1;
            }
            created_upstreams.push_back(upstream.name);

            for (const auto& server : upstream.servers)
            {
                if (RpcClient::add_upstream_server(upstream.name, server.address) != 0)
                {
                    std::cerr << "Failed to add server: " << server.address << std::endl;
                    cleanup_upstreams(created_upstreams);
                    return -1;
                }
            }
        }
        else if (upstream.type == "vnswrr")
        {
            if (RpcClient::create_vnswrr_upstream(upstream.name) != 0)
            {
                std::cerr << "Failed to create vnswrr upstream: " << upstream.name << std::endl;
                cleanup_upstreams(created_upstreams);
                return -1;
            }
            created_upstreams.push_back(upstream.name);

            for (const auto& server : upstream.servers)
            {
                if (RpcClient::add_upstream_server(upstream.name, server.address) != 0)
                {
                    std::cerr << "Failed to add server: " << server.address << std::endl;
                    cleanup_upstreams(created_upstreams);
                    return -1;
                }
            }
        }
    }

    int ret = GetRpcServer()->start(host.c_str(), port);
    if (ret != 0)
    {
        cleanup_upstreams(created_upstreams);
    }
    return ret;
}

static std::string trim(const std::string& s)
{
    auto start = std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    });
    auto end = std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base();
    if (start >= end)
        return "";
    return std::string(start, end);
}

static std::string get_tag_content(const std::string& content, const std::string& tag, size_t& pos)
{
    std::string start_tag = "<" + tag + ">";
    std::string end_tag = "</" + tag + ">";

    size_t start = content.find(start_tag, pos);
    if (start == std::string::npos)
        return "";

    start += start_tag.size();
    size_t end = content.find(end_tag, start);
    if (end == std::string::npos)
        return "";

    pos = end + end_tag.size();
    return trim(content.substr(start, end - start));
}

int parse_xml(const std::string& content, RpcConfig& config)
{
    size_t pos = 0;

    config.server.address = get_tag_content(content, "address", pos);
    config.server.protocol = get_tag_content(content, "protocol", pos);

    std::string iothread_str = get_tag_content(content, "iothread_num", pos);
    if (!iothread_str.empty())
    {
        try
        {
            config.server.iothread_num = std::stoi(iothread_str);
        }
        catch (...)
        {
            config.server.iothread_num = 0;
        }
    }

    std::string log_level = get_tag_content(content, "level", pos);
    if (!log_level.empty())
        config.log_level = log_level;

    std::string log_path = get_tag_content(content, "path", pos);
    if (!log_path.empty())
        config.log_path = log_path;

    size_t upstream_start = content.find("<upstream>", pos);
    while (upstream_start != std::string::npos)
    {
        size_t upstream_pos = upstream_start;
        UpstreamConfig upstream;

        upstream.name = get_tag_content(content, "name", upstream_pos);
        upstream.type = get_tag_content(content, "type", upstream_pos);

        std::string try_another_str = get_tag_content(content, "try_another", upstream_pos);
        upstream.try_another = (try_another_str == "true");

        size_t server_start = content.find("<server ", upstream_pos);
        size_t upstream_end = content.find("</upstream>", upstream_pos);

        while (server_start != std::string::npos && server_start < upstream_end)
        {
            UpstreamServerConfig server;

            size_t addr_start = content.find("address=\"", server_start);
            if (addr_start != std::string::npos && addr_start < upstream_end)
            {
                addr_start += 9;
                size_t addr_end = content.find("\"", addr_start);
                server.address = trim(content.substr(addr_start, addr_end - addr_start));
            }

            size_t weight_start = content.find("weight=\"", server_start);
            if (weight_start != std::string::npos && weight_start < upstream_end)
            {
                weight_start += 8;
                size_t weight_end = content.find("\"", weight_start);
                try
                {
                    server.weight = static_cast<unsigned short>(std::stoi(content.substr(weight_start, weight_end - weight_start)));
                }
                catch (...)
                {
                    server.weight = 1;
                }
            }
            else
            {
                server.weight = 1;
            }

            upstream.servers.push_back(server);

            server_start = content.find("<server ", server_start + 7);
        }

        config.upstreams.push_back(upstream);
        pos = upstream_end + 11;
        upstream_start = content.find("<upstream>", pos);
    }

    // 解析熔断器配置
    size_t circuit_breaker_start = content.find("<circuit_breaker>", pos);
    while (circuit_breaker_start != std::string::npos)
    {
        size_t cb_pos = circuit_breaker_start;
        CircuitBreakerConfig cb;

        cb.service_name = get_tag_content(content, "service_name", cb_pos);

        std::string failure_threshold_str = get_tag_content(content, "failure_threshold", cb_pos);
        if (!failure_threshold_str.empty())
        {
            try
            {
                cb.failure_threshold = std::stoi(failure_threshold_str);
            }
            catch (...)
            {
                cb.failure_threshold = 50;
            }
        }
        else
        {
            cb.failure_threshold = 50;
        }

        std::string success_threshold_str = get_tag_content(content, "success_threshold", cb_pos);
        if (!success_threshold_str.empty())
        {
            try
            {
                cb.success_threshold = std::stoi(success_threshold_str);
            }
            catch (...)
            {
                cb.success_threshold = 5;
            }
        }
        else
        {
            cb.success_threshold = 5;
        }

        std::string timeout_ms_str = get_tag_content(content, "timeout_ms", cb_pos);
        if (!timeout_ms_str.empty())
        {
            try
            {
                cb.timeout_ms = std::stoi(timeout_ms_str);
            }
            catch (...)
            {
                cb.timeout_ms = 30000;
            }
        }
        else
        {
            cb.timeout_ms = 30000;
        }

        config.circuit_breakers.push_back(cb);

        size_t circuit_breaker_end = content.find("</circuit_breaker>", cb_pos);
        pos = circuit_breaker_end + 18;
        circuit_breaker_start = content.find("<circuit_breaker>", pos);
    }

    // 解析限流器配置
    size_t rate_limiter_start = content.find("<rate_limiter>", pos);
    while (rate_limiter_start != std::string::npos)
    {
        size_t rl_pos = rate_limiter_start;
        RateLimiterConfig rl;

        rl.service_name = get_tag_content(content, "service_name", rl_pos);

        std::string qps_str = get_tag_content(content, "qps", rl_pos);
        if (!qps_str.empty())
        {
            try
            {
                rl.qps = std::stoi(qps_str);
            }
            catch (...)
            {
                rl.qps = 1000;
            }
        }
        else
        {
            rl.qps = 1000;
        }

        config.rate_limiters.push_back(rl);

        size_t rate_limiter_end = content.find("</rate_limiter>", rl_pos);
        pos = rate_limiter_end + 15;
        rate_limiter_start = content.find("<rate_limiter>", pos);
    }

    return 0;
}

std::string GetClientUpstreamUrl()
{
    const RpcConfig& config = GetConfig();

    // 如果配置文件中有upstream配置，则使用第一个upstream
    if (!config.upstreams.empty())
    {
        const UpstreamConfig& upstream = config.upstreams[0];
        
        // 注册upstream到Workflow框架
        std::vector<wf_rpc::UpstreamServer> servers;
        for (const auto& server : upstream.servers)
        {
            servers.push_back({server.address, server.weight});
        }
        
        if (wf_rpc::RpcClient::configure_weighted_upstream(upstream.name, servers, upstream.try_another) == 0)
        {
            // 返回tinypb://URL（TinyPB RPC已注册scheme支持）
            return "tinypb://" + upstream.name;
        }
        else
        {
            std::cerr << "Failed to configure upstream: " << upstream.name << "\n";
            return "";
        }
    }

    // 否则使用server配置中的地址
    if (!config.server.address.empty())
    {
        return config.server.address;
    }

    // 如果都没有配置，返回空字符串
    return "";
}

}