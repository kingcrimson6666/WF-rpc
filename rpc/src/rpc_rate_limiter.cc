#include "rpc_rate_limiter.h"
#include <chrono>

namespace wf_rpc
{

RateLimiter::RateLimiter(const std::string& name, int qps)
    : name_(name),
      qps_(qps),
      tokens_(qps),
      last_refill_time_(std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count())
{
}

bool RateLimiter::try_acquire(int permits)
{
    if (permits <= 0 || qps_.load() <= 0)
    {
        return false;
    }

    return try_acquire_internal(permits);
}

void RateLimiter::set_rate(int qps)
{
    qps_.store(qps);
    tokens_.store(qps);
}

int RateLimiter::get_rate() const
{
    return qps_.load();
}

const std::string& RateLimiter::get_name() const
{
    return name_;
}

bool RateLimiter::try_acquire_internal(int permits)
{
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    int64_t last_time = last_refill_time_.load();
    int qps = qps_.load();
    
    int64_t elapsed_ms = now - last_time;
    int64_t new_tokens = (elapsed_ms * qps) / 1000;
    
    if (new_tokens > 0)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_time = last_refill_time_.load();
        elapsed_ms = now - last_time;
        new_tokens = (elapsed_ms * qps) / 1000;
        
        if (new_tokens > 0)
        {
            int64_t current = tokens_.load();
            int64_t max_tokens = static_cast<int64_t>(qps);
            int64_t updated = std::min(current + new_tokens, max_tokens);
            tokens_.store(updated);
            last_refill_time_.store(now);
        }
    }
    
    int64_t current_tokens = tokens_.load();
    while (current_tokens >= permits)
    {
        if (tokens_.compare_exchange_weak(current_tokens, current_tokens - permits))
        {
            return true;
        }
    }
    
    return false;
}

}