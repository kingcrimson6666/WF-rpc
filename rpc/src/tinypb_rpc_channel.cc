#include "tinypb_rpc_channel.h"
#include <google/protobuf/descriptor.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFGlobal.h>
#include <workflow/EndpointParams.h>
#include <arpa/inet.h>
#include <atomic>
#include <cinttypes>
#include <memory>
#include <openssl/err.h>
#include <chrono>
#include "rpc_service_governance.h"
#include "rpc_metrics.h"
#include "rpc_logger.h"
#include "rpc_framework.h"

namespace wf_rpc
{

// 注册TinyPB RPC的scheme支持
static bool register_tinypb_scheme()
{
    // 注册tinypb://scheme，默认端口为20001
    WFGlobal::register_scheme_port("tinypb", 20001);
    WFGlobal::register_scheme_port("TinyPb", 20001);
    WFGlobal::register_scheme_port("TINYPB", 20001);
    
    // 注册tinypbs://scheme（带TLS），默认端口为20001
    WFGlobal::register_scheme_port("tinypbs", 20001);
    WFGlobal::register_scheme_port("TinyPbs", 20001);
    WFGlobal::register_scheme_port("TINYPBs", 20001);
    WFGlobal::register_scheme_port("TINYPBS", 20001);
    
    return true;
}

// 在全局初始化时注册scheme
static bool g_scheme_registered = register_tinypb_scheme();

static std::atomic<uint64_t> g_msg_req_seq(0);

static std::string generate_msg_req()
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%" PRIu64, g_msg_req_seq.fetch_add(1));
    return std::string(buf);
}

struct ChannelCallbackData {
    TinyPbRpcController* controller;
    google::protobuf::Message* response;
    google::protobuf::Closure* done;
    std::string service_name;
    std::string method_name;
    CircuitBreaker* breaker;
    std::chrono::steady_clock::time_point start_time;
};

static void channel_callback(WFNetworkTask<protocol::TLVMessage, protocol::TLVMessage>* task)
{
    std::unique_ptr<ChannelCallbackData> data(reinterpret_cast<ChannelCallbackData*>(task->user_data));

    // 记录请求结束时间
    auto end_time = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - data->start_time);

    if (task->get_state() != WFT_STATE_SUCCESS)
    {
        // 网络错误
        RPC_LOG_ERRORF("Network error: service=%s, method=%s, state=%d, error=%d, latency=%ldms",
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
        RPC_LOG_ERRORF("Failed to decode response: service=%s, method=%s, latency=%ldms",
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
        RPC_LOG_ERRORF("RPC error: service=%s, method=%s, err_code=%d, err_info=%s, latency=%ldms",
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

    if (!data->response->ParseFromString(response_struct.pb_data))
    {
        // 解析失败
        RPC_LOG_ERRORF("Failed to parse response: service=%s, method=%s, latency=%ldms",
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

    // 请求成功
    RPC_LOG_INFOF("RPC success: service=%s, method=%s, latency=%ldms",
                  data->service_name.c_str(), data->method_name.c_str(), latency.count());
    
    RpcMetrics::instance().record_success(data->service_name, data->method_name);
    RpcMetrics::instance().record_latency(data->service_name, data->method_name, latency.count());
    
    if (data->breaker)
        data->breaker->record_success();

    if (data->done)
        data->done->Run();
}

TinyPbRpcChannel::TinyPbRpcChannel(const std::string& host, unsigned short port)
    : host_(host), port_(port), use_upstream_(false), use_tls_(false), ssl_ctx_(nullptr)
{
}

TinyPbRpcChannel::TinyPbRpcChannel(const std::string& url)
    : url_(url), use_upstream_(true), use_tls_(false), ssl_ctx_(nullptr)
{
}

TinyPbRpcChannel::TinyPbRpcChannel(const std::string& host, unsigned short port, 
                                   const std::string& cert_file)
    : host_(host), port_(port), use_upstream_(false), use_tls_(true), 
      cert_file_(cert_file), ssl_ctx_(nullptr)
{
    init_ssl_ctx();
}

TinyPbRpcChannel::TinyPbRpcChannel(const std::string& host, unsigned short port, 
                                   const std::string& cert_file, const std::string& key_file)
    : host_(host), port_(port), use_upstream_(false), use_tls_(true), 
      cert_file_(cert_file), key_file_(key_file), ssl_ctx_(nullptr)
{
    init_ssl_ctx();
}

TinyPbRpcChannel::~TinyPbRpcChannel()
{
    if (ssl_ctx_)
    {
        SSL_CTX_free(ssl_ctx_);
    }
}

void TinyPbRpcChannel::init_ssl_ctx()
{
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    ssl_ctx_ = SSL_CTX_new(TLS_client_method());
    if (!ssl_ctx_)
    {
        ERR_print_errors_fp(stderr);
        return;
    }

    if (!cert_file_.empty())
    {
        if (SSL_CTX_load_verify_locations(ssl_ctx_, cert_file_.c_str(), nullptr) != 1)
        {
            std::cerr << "Warning: Failed to load certificate, continuing with SSL_VERIFY_NONE" << std::endl;
        }
    }

    SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_NONE, nullptr);
    SSL_CTX_set_options(ssl_ctx_, SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
    
    SSL_CTX_set_min_proto_version(ssl_ctx_, TLS1_2_VERSION);
    SSL_CTX_set_max_proto_version(ssl_ctx_, TLS1_3_VERSION);
}

void TinyPbRpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                                   google::protobuf::RpcController* controller,
                                   const google::protobuf::Message* request,
                                   google::protobuf::Message* response,
                                   google::protobuf::Closure* done)
{
    TinyPbRpcController* rpc_controller = dynamic_cast<TinyPbRpcController*>(controller);
    if (!rpc_controller)
    {
        if (controller)
            controller->SetFailed("invalid controller type");
        if (done)
            done->Run();
        return;
    }

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
            RPC_LOG_WARNF("Circuit breaker open: service=%s, method=%s",
                          service_name.c_str(), method_name.c_str());
            RpcMetrics::instance().record_failure(service_name, method_name);
            rpc_controller->SetErrorCode(RPC_CIRCUIT_BREAKER_OPEN);
            rpc_controller->SetFailed("circuit breaker open");
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
            RPC_LOG_WARNF("Rate limited: service=%s, method=%s",
                          service_name.c_str(), method_name.c_str());
            RpcMetrics::instance().record_failure(service_name, method_name);
            rpc_controller->SetErrorCode(RPC_RATE_LIMITED);
            rpc_controller->SetFailed("rate limited");
            if (done)
                done->Run();
            return;
        }
    }

    // 记录请求开始
    RPC_LOG_INFOF("Sending request: service=%s, method=%s",
                  service_name.c_str(), method_name.c_str());
    RpcMetrics::instance().record_request(service_name, method_name);

    std::string pb_data;
    if (!request->SerializeToString(&pb_data))
    {
        RPC_LOG_ERRORF("Failed to serialize request: service=%s, method=%s",
                       service_name.c_str(), method_name.c_str());
        RpcMetrics::instance().record_failure(service_name, method_name);
        if (breaker)
            breaker->record_failure();
        rpc_controller->SetFailed("failed to serialize request");
        if (done)
            done->Run();
        return;
    }

    std::string msg_req = generate_msg_req();
    rpc_controller->SetMsgReq(msg_req);

    TinyPbStruct request_struct(msg_req, service_full_name, 0, "", pb_data);

    std::string encoded_data;
    if (TinyPbCodec::encode(request_struct, encoded_data) != 0)
    {
        RPC_LOG_ERRORF("Failed to encode request: service=%s, method=%s",
                       service_name.c_str(), method_name.c_str());
        RpcMetrics::instance().record_failure(service_name, method_name);
        if (breaker)
            breaker->record_failure();
        rpc_controller->SetFailed("failed to encode request");
        if (done)
            done->Run();
        return;
    }

    using Factory = WFNetworkTaskFactory<protocol::TLVMessage, protocol::TLVMessage>;

    ChannelCallbackData* callback_data = new ChannelCallbackData;
    callback_data->controller = rpc_controller;
    callback_data->response = response;
    callback_data->done = done;
    callback_data->service_name = service_name;
    callback_data->method_name = method_name;
    callback_data->breaker = breaker;
    callback_data->start_time = std::chrono::steady_clock::now();

    WFNetworkTask<protocol::TLVMessage, protocol::TLVMessage>* task;
    enum TransportType transport_type = use_tls_ ? TT_TCP_SSL : TT_TCP;

    if (use_upstream_)
    {
        task = Factory::create_client_task(transport_type, url_.c_str(), 1, channel_callback);
    }
    else if (use_tls_ && ssl_ctx_)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);
        inet_pton(AF_INET, host_.c_str(), &addr.sin_addr);
        task = Factory::create_client_task(transport_type, 
                                           (const struct sockaddr*)&addr, 
                                           sizeof(addr),
                                           ssl_ctx_,
                                           1, 
                                           channel_callback);
    }
    else
    {
        task = Factory::create_client_task(transport_type, host_.c_str(), port_, 1, channel_callback);
    }

    task->user_data = callback_data;
    task->get_req()->set_value(std::move(encoded_data));
    task->set_keep_alive(30 * 1000);
    if (rpc_controller->GetTimeout() > 0)
        task->set_receive_timeout(rpc_controller->GetTimeout());
    task->start();
}

}