#ifndef WF_RPC_TINYPB_RPC_CHANNEL_H
#define WF_RPC_TINYPB_RPC_CHANNEL_H

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <string>
#include <memory>
#include "tinypb_codec.h"
#include "tinypb_rpc_controller.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/TLVMessage.h"

namespace wf_rpc
{

class TinyPbRpcChannel : public google::protobuf::RpcChannel
{
public:
    TinyPbRpcChannel(const std::string& host, unsigned short port);
    explicit TinyPbRpcChannel(const std::string& url);

    void CallMethod(const google::protobuf::MethodDescriptor* method,
                    google::protobuf::RpcController* controller,
                    const google::protobuf::Message* request,
                    google::protobuf::Message* response,
                    google::protobuf::Closure* done) override;

private:
    std::string host_;
    unsigned short port_;
    std::string url_;
    bool use_upstream_;
};

}

#endif