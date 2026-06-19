#include "tinypb_rpc_async_channel.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/stubs/callback.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFGlobal.h>
#include <atomic>
#include <cinttypes>
#include <memory>
#include <chrono>
#include "rpc_service_governance.h"
#include "rpc_metrics.h"
#include "rpc_logger.h"
#include "rpc_framework.h"

namespace wf_rpc
{

static std::atomic<uint64_t> g_async_msg_req_seq(0);

static std::string generate_async_msg_req()
{
    char buf[32];
    snprintf(buf, sizeof(buf), "async_%" PRIu64, g_async_msg_req_seq.fetch_add(1));
    return std::string(buf);
}

struct AsyncChannelCallbackData {
    TinyPbRpcController* controller;
    google::protobuf::Message* response;
    std::unique_ptr<google::protobuf::Message> resp_msg;
    google::protobuf::Closure* done;
    std::string service_name;
    std::string method_name;
    CircuitBreaker* breaker;
    std::chrono::steady_clock::time_point start_time;
};

static void async_channel_callback(WFNetworkTask<protocol::TLVMessage, protocol::TLVMessage>* task)
{
    std::unique_ptr<AsyncChannelCallbackData> data(reinterpret_cast<AsyncChannelCallbackData*>(task->user_data));
    
    // 记录请求结束时间
    auto end_time = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - data->start_time);

    if (task->get_state() != WFT_STATE_SUCCESS)
    {
        // 网络错误
        RPC_LOG_ERRORF("Async network error: service=%s, method=%s, state=%d, error=%d, latency=%ldms",
                       data->service_name.c_str(), data->method_name.c_str(),
                       task->get_state(), task->get_error(), latency.count());
        
        RpcMetrics::instance().record_failure(data->service_name, data->method_name);
        RpcMetrics::instance().record_latency(data->service_name, data->method_name, latency.count());
        
        if (data->breaker)
            data->breaker->record_failure();
        
        if (data->controller)
            data->controller->SetFailed("network error: " + std::to_string(task->get_error()));
        if (data->done)
            data->done->Run();
        return;
    }

    const std::string& resp_value = *task->get_resp()->get_value();

    TinyPbStruct response_struct;
    if (TinyPbCodec::decode(resp_value, response_struct) != 0)
    {
        // 解码失败
        RPC_LOG_ERRORF("Async failed to decode response: service=%s, method=%s, latency=%ldms",
                       data->service_name.c_str(), data->method_name.c_str(), latency.count());
        
        RpcMetrics::instance().record_failure(data->service_name, data->method_name);
        RpcMetrics::instance().record_latency(data->service_name, data->method_name, latency.count());
        
        if (data->breaker)
            data->breaker->record_failure();
        
        if (data->controller)
            data->controller->SetFailed("failed to decode response");
        if (data->done)
            data->done->Run();
        return;
    }

    if (response_struct.err_code != 0)
    {
        // RPC业务错误
        RPC_LOG_ERRORF("Async RPC error: service=%s, method=%s, err_code=%d, err_info=%s, latency=%ldms",
                       data->service_name.c_str(), data->method_name.c_str(),
                       response_struct.err_code, response_struct.err_info.c_str(), latency.count());
        
        RpcMetrics::instance().record_failure(data->service_name, data->method_name);
        RpcMetrics::instance().record_latency(data->service_name, data->method_name, latency.count());
        
        if (data->breaker)
            data->breaker->record_failure();
        
        if (data->controller)
        {
            data->controller->SetErrorCode(response_struct.err_code);
            data->controller->SetFailed(response_struct.err_info);
        }
        if (data->done)
            data->done->Run();
        return;
    }

    if (!data->resp_msg->ParseFromString(response_struct.pb_data))
    {
        // 解析失败
        RPC_LOG_ERRORF("Async failed to parse response: service=%s, method=%s, latency=%ldms",
                       data->service_name.c_str(), data->method_name.c_str(), latency.count());
        
        RpcMetrics::instance().record_failure(data->service_name, data->method_name);
        RpcMetrics::instance().record_latency(data->service_name, data->method_name, latency.count());
        
        if (data->breaker)
            data->breaker->record_failure();
        
        if (data->controller)
            data->controller->SetFailed("failed to parse response");
        if (data->done)
            data->done->Run();
        return;
    }

    data->response->CopyFrom(*data->resp_msg);

    // 请求成功
    RPC_LOG_INFOF("Async RPC success: service=%s, method=%s, latency=%ldms",
                  data->service_name.c_str(), data->method_name.c_str(), latency.count());
    
    RpcMetrics::instance().record_success(data->service_name, data->method_name);
    RpcMetrics::instance().record_latency(data->service_name, data->method_name, latency.count());
    
    if (data->breaker)
        data->breaker->record_success();

    if (data->done)
        data->done->Run();
}

TinyPbRpcAsyncChannel::TinyPbRpcAsyncChannel(const std::string& host, unsigned short port)
    : host_(host), port_(port), use_upstream_(false)
{
}

TinyPbRpcAsyncChannel::TinyPbRpcAsyncChannel(const std::string& url)
    : url_(url), use_upstream_(true)
{
}

void TinyPbRpcAsyncChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                                        google::protobuf::RpcController* controller,
                                        const google::protobuf::Message* request,
                                        google::protobuf::Message* response,
                                        google::protobuf::Closure* done)
{
    TinyPbRpcController* rpc_controller = dynamic_cast<TinyPbRpcController*>(controller);

    // 解析服务名和方法名
    std::string service_name = method->service()->full_name();
    std::string method_name = method->name();
    std::string service_full_name = service_name + "." + method_name;

    // 获取服务治理组件
    ServiceGovernanceManager& gov_manager = ServiceGovernanceManager::instance();
    CircuitBreaker* breaker = nullptr;
    RateLimiter* limiter = nullptr;

    // 检查熔断器
    if (gov_manager.is_circuit_breaker_enabled(service_name))
    {
        breaker = gov_manager.get_circuit_breaker(service_name);
        if (breaker && !breaker->allow_request())
        {
            RPC_LOG_WARNF("Async circuit breaker open: service=%s, method=%s",
                          service_name.c_str(), method_name.c_str());
            RpcMetrics::instance().record_failure(service_name, method_name);
            if (rpc_controller)
            {
                rpc_controller->SetErrorCode(RPC_CIRCUIT_BREAKER_OPEN);
                rpc_controller->SetFailed("circuit breaker open");
            }
            if (done)
                done->Run();
            return;
        }
    }

    // 检查限流器
    if (gov_manager.is_rate_limiter_enabled(service_name))
    {
        limiter = gov_manager.get_rate_limiter(service_name);
        if (limiter && !limiter->try_acquire(1))
        {
            RPC_LOG_WARNF("Async rate limited: service=%s, method=%s",
                          service_name.c_str(), method_name.c_str());
            RpcMetrics::instance().record_failure(service_name, method_name);
            if (rpc_controller)
            {
                rpc_controller->SetErrorCode(RPC_RATE_LIMITED);
                rpc_controller->SetFailed("rate limited");
            }
            if (done)
                done->Run();
            return;
        }
    }

    // 记录请求开始
    RPC_LOG_INFOF("Sending async request: service=%s, method=%s",
                  service_name.c_str(), method_name.c_str());
    RpcMetrics::instance().record_request(service_name, method_name);

    std::string pb_data;
    if (!request->SerializeToString(&pb_data))
    {
        RPC_LOG_ERRORF("Async failed to serialize request: service=%s, method=%s",
                       service_name.c_str(), method_name.c_str());
        RpcMetrics::instance().record_failure(service_name, method_name);
        if (breaker)
            breaker->record_failure();
        if (rpc_controller)
            rpc_controller->SetFailed("failed to serialize request");
        if (done)
            done->Run();
        return;
    }

    std::string msg_req = generate_async_msg_req();
    if (rpc_controller)
        rpc_controller->SetMsgReq(msg_req);

    TinyPbStruct request_struct(msg_req, service_full_name, 0, "", pb_data);

    std::string encoded_data;
    if (TinyPbCodec::encode(request_struct, encoded_data) != 0)
    {
        RPC_LOG_ERRORF("Async failed to encode request: service=%s, method=%s",
                       service_name.c_str(), method_name.c_str());
        RpcMetrics::instance().record_failure(service_name, method_name);
        if (breaker)
            breaker->record_failure();
        if (rpc_controller)
            rpc_controller->SetFailed("failed to encode request");
        if (done)
            done->Run();
        return;
    }

    using Factory = WFNetworkTaskFactory<protocol::TLVMessage, protocol::TLVMessage>;

    AsyncChannelCallbackData* callback_data = new AsyncChannelCallbackData;
    callback_data->controller = rpc_controller;
    callback_data->response = response;
    callback_data->resp_msg.reset(response->New());
    callback_data->done = done;
    callback_data->service_name = service_name;
    callback_data->method_name = method_name;
    callback_data->breaker = breaker;
    callback_data->start_time = std::chrono::steady_clock::now();

    WFNetworkTask<protocol::TLVMessage, protocol::TLVMessage>* task;
    if (use_upstream_)
    {
        task = Factory::create_client_task(TT_TCP, url_.c_str(), 1, async_channel_callback);
    }
    else
    {
        task = Factory::create_client_task(TT_TCP, host_.c_str(), port_, 1, async_channel_callback);
    }

    task->user_data = callback_data;
    task->get_req()->set_value(std::move(encoded_data));
    task->set_keep_alive(30 * 1000);
    if (rpc_controller && rpc_controller->GetTimeout() > 0)
        task->set_receive_timeout(rpc_controller->GetTimeout());
    task->start();
}

}