#ifndef WF_RPC_METRICS_H
#define WF_RPC_METRICS_H

#include <atomic>
#include <string>
#include <map>
#include <mutex>
#include <ctime>

namespace wf_rpc
{

class MetricsCounter
{
public:
    MetricsCounter() : count_(0) {}
    void inc(int64_t n = 1) { count_ += n; }
    int64_t get() const { return count_.load(); }
    void reset() { count_ = 0; }

private:
    std::atomic<int64_t> count_;
};

class MetricsHistogram
{
public:
    void record(int64_t value);
    int64_t get_sum() const { return sum_.load(); }
    int64_t get_count() const { return count_.load(); }
    int64_t get_min() const;
    int64_t get_max() const;

private:
    std::atomic<int64_t> sum_{0};
    std::atomic<int64_t> count_{0};
    std::atomic<int64_t> min_{INT64_MAX};
    std::atomic<int64_t> max_{INT64_MIN};
};

class MetricsGauge
{
public:
    void set(int64_t value) { value_ = value; }
    void inc(int64_t n = 1) { value_ += n; }
    void dec(int64_t n = 1) { value_ -= n; }
    int64_t get() const { return value_.load(); }

private:
    std::atomic<int64_t> value_;
};

class RpcMetrics
{
public:
    static RpcMetrics& instance();

    void record_request(const std::string& service, const std::string& method);
    void record_success(const std::string& service, const std::string& method);
    void record_failure(const std::string& service, const std::string& method);
    void record_latency(const std::string& service, const std::string& method, int64_t latency_ms);
    
    void set_active_connections(int count);
    void inc_active_connections();
    void dec_active_connections();
    
    std::string to_prometheus_format() const;

private:
    RpcMetrics() = default;
    ~RpcMetrics() = default;
    RpcMetrics(const RpcMetrics&) = delete;
    RpcMetrics& operator=(const RpcMetrics&) = delete;

private:
    mutable std::mutex mutex_;
    
    MetricsCounter total_requests_;
    MetricsCounter success_requests_;
    MetricsCounter failure_requests_;
    MetricsHistogram request_latency_;
    MetricsGauge active_connections_;
    
    std::map<std::string, MetricsCounter> service_requests_;
    std::map<std::string, MetricsCounter> service_successes_;
    std::map<std::string, MetricsCounter> service_failures_;
    std::map<std::string, MetricsHistogram> service_latencies_;
    
    std::atomic<int64_t> start_time_;
};

}

#endif