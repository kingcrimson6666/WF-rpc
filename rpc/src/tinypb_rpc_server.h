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
    void stop();

    TinyPbRpcDispatcher* get_dispatcher();

private:
    void on_process(WFNetworkTask<protocol::TLVMessage, protocol::TLVMessage>* task);

private:
    using ServerType = WFServer<protocol::TLVMessage, protocol::TLVMessage>;

    std::unique_ptr<ServerType> server_;
    std::unique_ptr<TinyPbRpcDispatcher> dispatcher_;
};

TinyPbRpcServer* GetRpcServer();

#define REGISTER_SERVICE(service_class) \
    { \
        google::protobuf::Service* service = new service_class(); \
        wf_rpc::GetRpcServer()->get_dispatcher()->registerService(service); \
    }

}

#endif