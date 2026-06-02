#include "tinypb_rpc_server.h"
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <mutex>
#include "tinypb_codec.h"
#include "rpc_service_registry.h"

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

    TinyPbStruct request_struct;
    if (TinyPbCodec::decode(req_value, request_struct) != 0)
    {
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

    TinyPbStruct response_struct;
    dispatcher_->dispatch(request_struct, response_struct);

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