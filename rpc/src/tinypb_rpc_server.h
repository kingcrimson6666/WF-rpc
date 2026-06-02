#ifndef WF_RPC_TINYPB_RPC_SERVER_H
#define WF_RPC_TINYPB_RPC_SERVER_H

#include <string>
#include <memory>
#include <workflow/WFServer.h>
#include <workflow/TLVMessage.h>
#include "tinypb_rpc_dispatcher.h"

namespace wf_rpc
{

class TinyPbRpcServer
{
public:
    TinyPbRpcServer();
    explicit TinyPbRpcServer(const struct WFServerParams* params);
    ~TinyPbRpcServer();

    int start(unsigned short port);
    int start(int family, unsigned short port);
    int start(const char* host, unsigned short port);
    
    int start(unsigned short port, const std::string& cert_file, const std::string& key_file);
    int start(int family, unsigned short port, const std::string& cert_file, const std::string& key_file);
    int start(const char* host, unsigned short port, const std::string& cert_file, const std::string& key_file);
    
    void stop();

    TinyPbRpcDispatcher* get_dispatcher();
    
    void set_registry_enabled(bool enable);
    void set_service_name(const std::string& name);
    void set_registry_endpoint(const std::string& endpoint);
    bool is_registered() const { return registered_; }

private:
    bool register_to_registry(const char* host, unsigned short port);
    void unregister_from_registry();

private:
    void on_process(WFNetworkTask<protocol::TLVMessage, protocol::TLVMessage>* task);

private:
    using ServerType = WFServer<protocol::TLVMessage, protocol::TLVMessage>;

    std::unique_ptr<ServerType> server_;
    std::unique_ptr<TinyPbRpcDispatcher> dispatcher_;
    bool registry_enabled_;
    std::string service_name_;
    std::string host_;
    unsigned short port_;
    bool registered_;
};

TinyPbRpcServer* GetRpcServer();

#define REGISTER_SERVICE(service_class) \
    { \
        google::protobuf::Service* service = new service_class(); \
        wf_rpc::GetRpcServer()->get_dispatcher()->registerService(service); \
    }

}

#endif