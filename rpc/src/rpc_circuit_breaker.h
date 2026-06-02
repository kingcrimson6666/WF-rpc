#ifndef WF_RPC_CIRCUIT_BREAKER_H
#define WF_RPC_CIRCUIT_BREAKER_H

#include <atomic>
#include <mutex>
#include <chrono>
#include <string>

namespace wf_rpc
{

class CircuitBreaker
{
public:
    enum State {
        STATE_CLOSED = 0,
        STATE_OPEN = 1,
        STATE_HALF_OPEN = 2
    };

    CircuitBreaker(const std::string& name,
                   int failure_threshold = 50,
                   int success_threshold = 5,
                   int timeout_ms = 30000);

    ~CircuitBreaker() = default;

    bool allow_request();
    void record_success();
    void record_failure();
    State get_state() const;
    const std::string& get_name() const;

private:
    void transition_to_open();
    void transition_to_closed();
    void transition_to_half_open();
    bool should_attempt_reset();

private:
    std::string name_;
    std::atomic<State> state_;
    std::atomic<int> failure_count_;
    std::atomic<int> success_count_;
    std::atomic<int64_t> last_failure_time_;
    
    int failure_threshold_;
    int success_threshold_;
    int timeout_ms_;
    
    std::mutex mutex_;
};

}

#endif