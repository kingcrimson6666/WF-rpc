#include "rpc_circuit_breaker.h"
#include <chrono>

namespace wf_rpc
{

CircuitBreaker::CircuitBreaker(const std::string& name,
                               int failure_threshold,
                               int success_threshold,
                               int timeout_ms)
    : name_(name),
      state_(STATE_CLOSED),
      failure_count_(0),
      success_count_(0),
      last_failure_time_(0),
      failure_threshold_(failure_threshold),
      success_threshold_(success_threshold),
      timeout_ms_(timeout_ms)
{
}

bool CircuitBreaker::allow_request()
{
    State current = state_.load();
    
    if (current == STATE_CLOSED)
    {
        return true;
    }
    
    if (current == STATE_OPEN)
    {
        if (should_attempt_reset())
        {
            transition_to_half_open();
            return true;
        }
        return false;
    }
    
    if (current == STATE_HALF_OPEN)
    {
        return true;
    }
    
    return false;
}

void CircuitBreaker::record_success()
{
    State current = state_.load();
    
    if (current == STATE_CLOSED)
    {
        failure_count_.store(0);
    }
    else if (current == STATE_HALF_OPEN)
    {
        int count = success_count_.fetch_add(1) + 1;
        if (count >= success_threshold_)
        {
            transition_to_closed();
        }
    }
}

void CircuitBreaker::record_failure()
{
    State current = state_.load();
    
    if (current == STATE_CLOSED)
    {
        int count = failure_count_.fetch_add(1) + 1;
        if (count >= failure_threshold_)
        {
            transition_to_open();
        }
    }
    else if (current == STATE_HALF_OPEN)
    {
        transition_to_open();
    }
}

CircuitBreaker::State CircuitBreaker::get_state() const
{
    return state_.load();
}

const std::string& CircuitBreaker::get_name() const
{
    return name_;
}

void CircuitBreaker::transition_to_open()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_.load() != STATE_OPEN)
    {
        state_.store(STATE_OPEN);
        last_failure_time_.store(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }
}

void CircuitBreaker::transition_to_closed()
{
    std::lock_guard<std::mutex> lock(mutex_);
    state_.store(STATE_CLOSED);
    failure_count_.store(0);
    success_count_.store(0);
}

void CircuitBreaker::transition_to_half_open()
{
    std::lock_guard<std::mutex> lock(mutex_);
    state_.store(STATE_HALF_OPEN);
    success_count_.store(0);
}

bool CircuitBreaker::should_attempt_reset()
{
    int64_t last_time = last_failure_time_.load();
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return (now - last_time) >= timeout_ms_;
}

}