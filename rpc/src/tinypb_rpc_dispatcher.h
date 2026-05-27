#ifndef WF_RPC_TINYPB_RPC_DISPATCHER_H
#define WF_RPC_TINYPB_RPC_DISPATCHER_H

#include <unordered_map>
#include <memory>
#include <string>
#include <mutex>
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include "tinypb_struct.h"
#include "workflow/WFTaskFactory.h"

namespace wf_rpc
{

class TinyPbRpcDispatcher
{
public:
    TinyPbRpcDispatcher() = default;
    ~TinyPbRpcDispatcher() = default;

    void registerService(google::protobuf::Service* service);
    google::protobuf::Service* getService(const std::string& service_name);

    void dispatch(const TinyPbStruct& request, TinyPbStruct& response);

private:
    std::unordered_map<std::string, std::unique_ptr<google::protobuf::Service>> service_map_;
    mutable std::mutex mutex_;
};

}

#endif