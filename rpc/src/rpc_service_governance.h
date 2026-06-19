#ifndef WF_RPC_SERVICE_GOVERNANCE_H
#define WF_RPC_SERVICE_GOVERNANCE_H

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include "rpc_circuit_breaker.h"
#include "rpc_rate_limiter.h"

namespace wf_rpc
{

class ServiceGovernanceManager
{
public:
    static ServiceGovernanceManager& instance();

    // 获取或创建熔断器
    CircuitBreaker* get_circuit_breaker(const std::string& service_name,
                                        int failure_threshold = 50,
                                        int success_threshold = 5,
                                        int timeout_ms = 30000);

    // 获取或创建限流器
    RateLimiter* get_rate_limiter(const std::string& service_name, int qps = 1000);

    // 设置熔断器参数
    void set_circuit_breaker_params(const std::string& service_name,
                                    int failure_threshold,
                                    int success_threshold,
                                    int timeout_ms);

    // 设置限流器参数
    void set_rate_limiter_params(const std::string& service_name, int qps);

    // 启用/禁用熔断器
    void enable_circuit_breaker(const std::string& service_name, bool enable);
    void disable_circuit_breaker(const std::string& service_name);

    // 启用/禁用限流器
    void enable_rate_limiter(const std::string& service_name, bool enable);
    void disable_rate_limiter(const std::string& service_name);

    // 检查是否启用熔断器
    bool is_circuit_breaker_enabled(const std::string& service_name);

    // 检查是否启用限流器
    bool is_rate_limiter_enabled(const std::string& service_name);

private:
    ServiceGovernanceManager() = default;
    ~ServiceGovernanceManager() = default;
    ServiceGovernanceManager(const ServiceGovernanceManager&) = delete;
    ServiceGovernanceManager& operator=(const ServiceGovernanceManager&) = delete;

private:
    mutable std::mutex mutex_;
    
    std::unordered_map<std::string, std::unique_ptr<CircuitBreaker>> circuit_breakers_;
    std::unordered_map<std::string, std::unique_ptr<RateLimiter>> rate_limiters_;
    
    std::unordered_map<std::string, bool> circuit_breaker_enabled_;
    std::unordered_map<std::string, bool> rate_limiter_enabled_;
};

}

#endif