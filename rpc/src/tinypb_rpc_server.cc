#include "tinypb_rpc_server.h"
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <mutex>
#include "tinypb_codec.h"

namespace wf_rpc
{

static std::unique_ptr<TinyPbRpcServer> g_rpc_server;
static std::once_flag g_rpc_server_init_flag;

TinyPbRpcServer::TinyPbRpcServer()
    : server_(new ServerType(std::bind(&TinyPbRpcServer::on_process, this, std::placeholders::_1))),
      dispatcher_(new TinyPbRpcDispatcher())
{
}

TinyPbRpcServer::TinyPbRpcServer(const struct WFServerParams* params)
    : server_(new ServerType(params, std::bind(&TinyPbRpcServer::on_process, this, std::placeholders::_1))),
      dispatcher_(new TinyPbRpcDispatcher())
{
}

TinyPbRpcServer::~TinyPbRpcServer()
{
}

int TinyPbRpcServer::start(unsigned short port)
{
    return server_->start(port);
}

int TinyPbRpcServer::start(int family, unsigned short port)
{
    return server_->start(family, port);
}

int TinyPbRpcServer::start(const char* host, unsigned short port)
{
    return server_->start(host, port);
}

void TinyPbRpcServer::stop()
{
    server_->stop();
}

TinyPbRpcDispatcher* TinyPbRpcServer::get_dispatcher()
{
    return dispatcher_.get();
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