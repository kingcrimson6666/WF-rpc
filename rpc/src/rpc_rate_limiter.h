#ifndef WF_RPC_RATE_LIMITER_H
#define WF_RPC_RATE_LIMITER_H

#include <atomic>
#include <mutex>
#include <chrono>
#include <string>

namespace wf_rpc
{

class RateLimiter
{
public:
    RateLimiter(const std::string& name, int qps);
    ~RateLimiter() = default;

    bool try_acquire(int permits = 1);
    void set_rate(int qps);
    int get_rate() const;
    const std::string& get_name() const;

private:
    bool try_acquire_internal(int permits);

private:
    std::string name_;
    std::atomic<int> qps_;
    std::atomic<int64_t> tokens_;
    std::atomic<int64_t> last_refill_time_;
    
    std::mutex mutex_;
};

}

#endif