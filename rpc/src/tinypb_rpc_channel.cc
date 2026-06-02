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

namespace wf_rpc
{

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
};

static void channel_callback(WFNetworkTask<protocol::TLVMessage, protocol::TLVMessage>* task)
{
    std::unique_ptr<ChannelCallbackData> data(reinterpret_cast<ChannelCallbackData*>(task->user_data));

    if (task->get_state() != WFT_STATE_SUCCESS)
    {
        std::cerr << "Network error: state=" << task->get_state() << ", error=" << task->get_error() << std::endl;
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

    if (!data->response->ParseFromString(response_struct.pb_data))
    {
        if (data->controller)
            data->controller->SetFailed("failed to parse response");
        if (data->done)
            data->done->Run();
        return;
    }

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

    std::string service_full_name = method->service()->full_name() + "." + method->name();

    std::string pb_data;
    if (!request->SerializeToString(&pb_data))
    {
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