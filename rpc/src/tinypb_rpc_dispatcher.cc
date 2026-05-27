#include "tinypb_rpc_dispatcher.h"

namespace wf_rpc
{

void TinyPbRpcDispatcher::registerService(google::protobuf::Service* service)
{
    const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
    std::string service_name = desc->full_name();
    std::lock_guard<std::mutex> lock(mutex_);
    service_map_[service_name] = std::unique_ptr<google::protobuf::Service>(service);
}

google::protobuf::Service* TinyPbRpcDispatcher::getService(const std::string& service_name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = service_map_.find(service_name);
    if (it != service_map_.end())
    {
        return it->second.get();
    }
    return nullptr;
}

void TinyPbRpcDispatcher::dispatch(const TinyPbStruct& request, TinyPbStruct& response)
{
    response.msg_req = request.msg_req;
    response.service_full_name = request.service_full_name;

    size_t dot_pos = request.service_full_name.rfind('.');
    if (dot_pos == std::string::npos)
    {
        response.err_code = 400;
        response.err_info = "invalid service_full_name format";
        return;
    }

    std::string service_name = request.service_full_name.substr(0, dot_pos);
    std::string method_name = request.service_full_name.substr(dot_pos + 1);

    google::protobuf::Service* service = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = service_map_.find(service_name);
        if (it == service_map_.end())
        {
            response.err_code = 404;
            response.err_info = "service not found: " + service_name;
            return;
        }
        service = it->second.get();
    }

    const google::protobuf::ServiceDescriptor* service_desc = service->GetDescriptor();
    const google::protobuf::MethodDescriptor* method_desc = service_desc->FindMethodByName(method_name);

    if (!method_desc)
    {
        response.err_code = 404;
        response.err_info = "method not found: " + method_name;
        return;
    }

    std::unique_ptr<google::protobuf::Message> req_msg(service->GetRequestPrototype(method_desc).New());
    std::unique_ptr<google::protobuf::Message> resp_msg(service->GetResponsePrototype(method_desc).New());

    if (!req_msg->ParseFromString(request.pb_data))
    {
        response.err_code = 422;
        response.err_info = "failed to parse request";
        return;
    }

    service->CallMethod(method_desc, nullptr, req_msg.get(), resp_msg.get(), nullptr);

    if (!resp_msg->SerializeToString(&response.pb_data))
    {
        response.err_code = 500;
        response.err_info = "failed to serialize response";
    }
    else
    {
        response.err_code = 0;
        response.err_info = "";
    }
}

}