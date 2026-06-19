#include "rpc_service_governance.h"

namespace wf_rpc
{

ServiceGovernanceManager& ServiceGovernanceManager::instance()
{
    static ServiceGovernanceManager instance;
    return instance;
}

CircuitBreaker* ServiceGovernanceManager::get_circuit_breaker(const std::string& service_name,
                                                              int failure_threshold,
                                                              int success_threshold,
                                                              int timeout_ms)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = circuit_breakers_.find(service_name);
    if (it != circuit_breakers_.end())
    {
        return it->second.get();
    }
    
    auto breaker = std::unique_ptr<CircuitBreaker>(
        new CircuitBreaker(service_name, failure_threshold, success_threshold, timeout_ms));
    circuit_breakers_[service_name] = std::move(breaker);
    circuit_breaker_enabled_[service_name] = true;  // 默认启用
    
    return circuit_breakers_[service_name].get();
}

RateLimiter* ServiceGovernanceManager::get_rate_limiter(const std::string& service_name, int qps)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = rate_limiters_.find(service_name);
    if (it != rate_limiters_.end())
    {
        return it->second.get();
    }
    
    auto limiter = std::unique_ptr<RateLimiter>(new RateLimiter(service_name, qps));
    rate_limiters_[service_name] = std::move(limiter);
    rate_limiter_enabled_[service_name] = true;  // 默认启用
    
    return rate_limiters_[service_name].get();
}

void ServiceGovernanceManager::set_circuit_breaker_params(const std::string& service_name,
                                                          int failure_threshold,
                                                          int success_threshold,
                                                          int timeout_ms)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = circuit_breakers_.find(service_name);
    if (it != circuit_breakers_.end())
    {
        // 重新创建熔断器以更新参数
        circuit_breakers_[service_name] = std::unique_ptr<CircuitBreaker>(
            new CircuitBreaker(service_name, failure_threshold, success_threshold, timeout_ms));
    }
}

void ServiceGovernanceManager::set_rate_limiter_params(const std::string& service_name, int qps)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = rate_limiters_.find(service_name);
    if (it != rate_limiters_.end())
    {
        it->second->set_rate(qps);
    }
}

void ServiceGovernanceManager::enable_circuit_breaker(const std::string& service_name, bool enable)
{
    std::lock_guard<std::mutex> lock(mutex_);
    circuit_breaker_enabled_[service_name] = enable;
}

void ServiceGovernanceManager::disable_circuit_breaker(const std::string& service_name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    circuit_breaker_enabled_[service_name] = false;
}

void ServiceGovernanceManager::enable_rate_limiter(const std::string& service_name, bool enable)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rate_limiter_enabled_[service_name] = enable;
}

void ServiceGovernanceManager::disable_rate_limiter(const std::string& service_name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rate_limiter_enabled_[service_name] = false;
}

bool ServiceGovernanceManager::is_circuit_breaker_enabled(const std::string& service_name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = circuit_breaker_enabled_.find(service_name);
    return it != circuit_breaker_enabled_.end() && it->second;
}

bool ServiceGovernanceManager::is_rate_limiter_enabled(const std::string& service_name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rate_limiter_enabled_.find(service_name);
    return it != rate_limiter_enabled_.end() && it->second;
}

}