#include "tinypb_rpc_server.h"
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <mutex>
#include <chrono>
#include "tinypb_codec.h"
#include "rpc_service_registry.h"
#include "rpc_metrics.h"
#include "rpc_logger.h"

namespace wf_rpc
{

static std::unique_ptr<TinyPbRpcServer> g_rpc_server;
static std::once_flag g_rpc_server_init_flag;

TinyPbRpcServer::TinyPbRpcServer()
    : server_(new ServerType(std::bind(&TinyPbRpcServer::on_process, this, std::placeholders::_1))),
      dispatcher_(new TinyPbRpcDispatcher()),
      registry_enabled_(false),
      registered_(false)
{
}

TinyPbRpcServer::TinyPbRpcServer(const struct WFServerParams* params)
    : server_(new ServerType(params, std::bind(&TinyPbRpcServer::on_process, this, std::placeholders::_1))),
      dispatcher_(new TinyPbRpcDispatcher()),
      registry_enabled_(false),
      registered_(false)
{
}

TinyPbRpcServer::~TinyPbRpcServer()
{
    stop();
}

int TinyPbRpcServer::start(unsigned short port)
{
    return start("0.0.0.0", port);
}

int TinyPbRpcServer::start(int family, unsigned short port)
{
    if (registry_enabled_ && !service_name_.empty())
    {
        register_to_registry("0.0.0.0", port);
    }
    host_ = "0.0.0.0";
    port_ = port;
    return server_->start(family, port);
}

int TinyPbRpcServer::start(const char* host, unsigned short port)
{
    if (registry_enabled_ && !service_name_.empty())
    {
        register_to_registry(host, port);
    }
    host_ = host;
    port_ = port;
    return server_->start(host, port);
}

int TinyPbRpcServer::start(unsigned short port, const std::string& cert_file, const std::string& key_file)
{
    return start("0.0.0.0", port, cert_file, key_file);
}

int TinyPbRpcServer::start(int family, unsigned short port, const std::string& cert_file, const std::string& key_file)
{
    if (registry_enabled_ && !service_name_.empty())
    {
        register_to_registry("0.0.0.0", port);
    }
    host_ = "0.0.0.0";
    port_ = port;
    return server_->start(family, port, cert_file.c_str(), key_file.c_str());
}

int TinyPbRpcServer::start(const char* host, unsigned short port, const std::string& cert_file, const std::string& key_file)
{
    if (registry_enabled_ && !service_name_.empty())
    {
        register_to_registry(host, port);
    }
    host_ = host;
    port_ = port;
    return server_->start(host, port, cert_file.c_str(), key_file.c_str());
}

void TinyPbRpcServer::stop()
{
    if (registered_)
    {
        unregister_from_registry();
    }
    server_->stop();
}

TinyPbRpcDispatcher* TinyPbRpcServer::get_dispatcher()
{
    return dispatcher_.get();
}

void TinyPbRpcServer::set_registry_enabled(bool enable)
{
    registry_enabled_ = enable;
}

void TinyPbRpcServer::set_service_name(const std::string& name)
{
    service_name_ = name;
}

void TinyPbRpcServer::set_registry_endpoint(const std::string& endpoint)
{
    EtcdRegistryClient::instance().set_endpoint(endpoint);
    EtcdRegistryClient::instance().start();
}

bool TinyPbRpcServer::register_to_registry(const char* host, unsigned short port)
{
    if (service_name_.empty())
    {
        std::cerr << "Service name not set, cannot register to registry" << std::endl;
        return false;
    }
    
    bool success = EtcdRegistryClient::instance().register_service(service_name_, host, port, 30);
    if (success)
    {
        registered_ = true;
        std::cout << "Service registered to registry: " << service_name_ << " -> " << host << ":" << port << std::endl;
    }
    else
    {
        std::cerr << "Failed to register service to registry" << std::endl;
    }
    return success;
}

void TinyPbRpcServer::unregister_from_registry()
{
    if (!service_name_.empty() && registered_)
    {
        EtcdRegistryClient::instance().unregister_service(service_name_);
        registered_ = false;
        std::cout << "Service unregistered from registry: " << service_name_ << std::endl;
    }
}

void TinyPbRpcServer::on_process(WFNetworkTask<protocol::TLVMessage, protocol::TLVMessage>* task)
{
    protocol::TLVMessage* req = task->get_req();
    protocol::TLVMessage* resp = task->get_resp();

    std::string& req_value = *req->get_value();

    // 记录请求开始时间
    auto start_time = std::chrono::steady_clock::now();

    TinyPbStruct request_struct;
    if (TinyPbCodec::decode(req_value, request_struct) != 0)
    {
        // 记录解码失败
        RPC_LOG_ERROR("Failed to decode request");
        RpcMetrics::instance().record_failure("unknown", "unknown");

        TinyPbStruct response_struct;
        response_struct.msg_req = "unknown";
        response_struct.service_full_name = "";
        response_struct.err_code = 400;
        response_struct.err_info = "invalid request";

        std::string encoded;
        if (TinyPbCodec::encode(response_struct, encoded) == 0)
        {
            resp->set_value(std::move(encoded));
        }
        return;
    }

    // 解析服务名和方法名
    std::string service_name;
    std::string method_name;
    size_t pos = request_struct.service_full_name.find('.');
    if (pos != std::string::npos)
    {
        service_name = request_struct.service_full_name.substr(0, pos);
        method_name = request_struct.service_full_name.substr(pos + 1);
    }
    else
    {
        service_name = request_struct.service_full_name;
        method_name = "unknown";
    }

    // 记录请求开始
    RPC_LOG_INFOF("Received request: service=%s, method=%s, msg_req=%s",
                  service_name.c_str(), method_name.c_str(), request_struct.msg_req.c_str());
    RpcMetrics::instance().record_request(service_name, method_name);

    TinyPbStruct response_struct;
    dispatcher_->dispatch(request_struct, response_struct);

    // 记录请求结束时间
    auto end_time = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // 根据响应结果记录指标和日志
    if (response_struct.err_code == 0)
    {
        // 请求成功
        RpcMetrics::instance().record_success(service_name, method_name);
        RpcMetrics::instance().record_latency(service_name, method_name, latency.count());
        RPC_LOG_INFOF("Request success: service=%s, method=%s, latency=%ldms",
                      service_name.c_str(), method_name.c_str(), latency.count());
    }
    else
    {
        // 请求失败
        RpcMetrics::instance().record_failure(service_name, method_name);
        RpcMetrics::instance().record_latency(service_name, method_name, latency.count());
        RPC_LOG_ERRORF("Request failed: service=%s, method=%s, err_code=%d, err_info=%s, latency=%ldms",
                       service_name.c_str(), method_name.c_str(), response_struct.err_code,
                       response_struct.err_info.c_str(), latency.count());
    }

    std::string encoded;
    if (TinyPbCodec::encode(response_struct, encoded) == 0)
    {
        resp->set_value(std::move(encoded));
    }
}

TinyPbRpcServer* GetRpcServer()
{
    std::call_once(g_rpc_server_init_flag, []() {
        g_rpc_server.reset(new TinyPbRpcServer());
    });
    return g_rpc_server.get();
}

}