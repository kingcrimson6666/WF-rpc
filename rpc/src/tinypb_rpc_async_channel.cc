#include "tinypb_rpc_async_channel.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/stubs/callback.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFGlobal.h>
#include <atomic>
#include <cinttypes>
#include <memory>

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
};

static void async_channel_callback(WFNetworkTask<protocol::TLVMessage, protocol::TLVMessage>* task)
{
    std::unique_ptr<AsyncChannelCallbackData> data(reinterpret_cast<AsyncChannelCallbackData*>(task->user_data));
    
    if (task->get_state() != WFT_STATE_SUCCESS)
    {
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
        if (data->controller)
            data->controller->SetFailed("failed to decode response");
        if (data->done)
            data->done->Run();
        return;
    }

    if (response_struct.err_code != 0)
    {
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
        if (data->controller)
            data->controller->SetFailed("failed to parse response");
        if (data->done)
            data->done->Run();
        return;
    }

    data->response->CopyFrom(*data->resp_msg);

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

    std::string service_full_name = method->service()->full_name() + "." + method->name();

    std::string pb_data;
    if (!request->SerializeToString(&pb_data))
    {
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