#include "rpc_metrics.h"
#include <sstream>
#include <iomanip>

namespace wf_rpc
{

void MetricsHistogram::record(int64_t value)
{
    sum_ += value;
    count_++;
    
    int64_t current_min = min_.load();
    while (value < current_min && !min_.compare_exchange_weak(current_min, value)) {}
    
    int64_t current_max = max_.load();
    while (value > current_max && !max_.compare_exchange_weak(current_max, value)) {}
}

int64_t MetricsHistogram::get_min() const
{
    int64_t val = min_.load();
    return val == INT64_MAX ? 0 : val;
}

int64_t MetricsHistogram::get_max() const
{
    int64_t val = max_.load();
    return val == INT64_MIN ? 0 : val;
}

RpcMetrics& RpcMetrics::instance()
{
    static RpcMetrics instance;
    return instance;
}

void RpcMetrics::record_request(const std::string& service, const std::string& method)
{
    total_requests_.inc();
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = service + "_" + method;
    service_requests_[key].inc();
}

void RpcMetrics::record_success(const std::string& service, const std::string& method)
{
    success_requests_.inc();
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = service + "_" + method;
    service_successes_[key].inc();
}

void RpcMetrics::record_failure(const std::string& service, const std::string& method)
{
    failure_requests_.inc();
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = service + "_" + method;
    service_failures_[key].inc();
}

void RpcMetrics::record_latency(const std::string& service, const std::string& method, int64_t latency_ms)
{
    request_latency_.record(latency_ms);
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = service + "_" + method;
    service_latencies_[key].record(latency_ms);
}

void RpcMetrics::set_active_connections(int count)
{
    active_connections_.set(count);
}

void RpcMetrics::inc_active_connections()
{
    active_connections_.inc();
}

void RpcMetrics::dec_active_connections()
{
    active_connections_.dec();
}

std::string RpcMetrics::to_prometheus_format() const
{
    std::ostringstream oss;
    std::time_t now = std::time(nullptr);
    
    oss << "# HELP wf_rpc_total_requests Total RPC requests\n";
    oss << "# TYPE wf_rpc_total_requests counter\n";
    oss << "wf_rpc_total_requests " << total_requests_.get() << " " << now << "\n\n";
    
    oss << "# HELP wf_rpc_success_requests Successful RPC requests\n";
    oss << "# TYPE wf_rpc_success_requests counter\n";
    oss << "wf_rpc_success_requests " << success_requests_.get() << " " << now << "\n\n";
    
    oss << "# HELP wf_rpc_failure_requests Failed RPC requests\n";
    oss << "# TYPE wf_rpc_failure_requests counter\n";
    oss << "wf_rpc_failure_requests " << failure_requests_.get() << " " << now << "\n\n";
    
    oss << "# HELP wf_rpc_request_latency_ms RPC request latency in milliseconds\n";
    oss << "# TYPE wf_rpc_request_latency_ms summary\n";
    oss << "wf_rpc_request_latency_ms_sum " << request_latency_.get_sum() << " " << now << "\n";
    oss << "wf_rpc_request_latency_ms_count " << request_latency_.get_count() << " " << now << "\n";
    oss << "wf_rpc_request_latency_ms_min " << request_latency_.get_min() << " " << now << "\n";
    oss << "wf_rpc_request_latency_ms_max " << request_latency_.get_max() << " " << now << "\n\n";
    
    oss << "# HELP wf_rpc_active_connections Current active connections\n";
    oss << "# TYPE wf_rpc_active_connections gauge\n";
    oss << "wf_rpc_active_connections " << active_connections_.get() << " " << now << "\n\n";
    
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& pair : service_requests_) {
        oss << "# HELP wf_rpc_service_requests_total Total requests per service\n";
        oss << "# TYPE wf_rpc_service_requests_total counter\n";
        oss << "wf_rpc_service_requests_total{service=\"" << pair.first << "\"} " 
            << pair.second.get() << " " << now << "\n";
    }
    
    for (const auto& pair : service_successes_) {
        oss << "# HELP wf_rpc_service_successes_total Successful requests per service\n";
        oss << "# TYPE wf_rpc_service_successes_total counter\n";
        oss << "wf_rpc_service_successes_total{service=\"" << pair.first << "\"} " 
            << pair.second.get() << " " << now << "\n";
    }
    
    for (const auto& pair : service_failures_) {
        oss << "# HELP wf_rpc_service_failures_total Failed requests per service\n";
        oss << "# TYPE wf_rpc_service_failures_total counter\n";
        oss << "wf_rpc_service_failures_total{service=\"" << pair.first << "\"} " 
            << pair.second.get() << " " << now << "\n";
    }
    
    return oss.str();
}

}