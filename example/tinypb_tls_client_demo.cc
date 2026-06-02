#include <iostream>
#include <workflow/WFFacilities.h>
#include "echo.pb.h"
#include "tinypb_rpc_channel.h"
#include "tinypb_rpc_controller.h"

static WFFacilities::WaitGroup* wg_ptr = nullptr;

void rpc_callback()
{
    wg_ptr->done();
}

int main(int argc, char* argv[])
{
    if (argc != 4 && argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " <host> <port> <cert_file> [key_file]\n";
        return 1;
    }

    const char* host = argv[1];
    unsigned short port = (unsigned short)atoi(argv[2]);
    const std::string cert_file = argv[3];
    std::string key_file;
    
    if (argc == 5)
    {
        key_file = argv[4];
    }

    WFFacilities::WaitGroup wg(1);
    wg_ptr = &wg;

    std::unique_ptr<wf_rpc::TinyPbRpcChannel> channel;
    
    if (key_file.empty())
    {
        channel.reset(new wf_rpc::TinyPbRpcChannel(host, port, cert_file));
    }
    else
    {
        channel.reset(new wf_rpc::TinyPbRpcChannel(host, port, cert_file, key_file));
    }

    wf::rpc::example::EchoService_Stub stub(channel.get());

    wf_rpc::TinyPbRpcController controller;
    controller.SetTimeout(5000);

    wf::rpc::example::EchoRequest request;
    request.set_message("hello_tls");

    wf::rpc::example::EchoResponse response;

    auto done = google::protobuf::NewCallback(rpc_callback);

    stub.Echo(&controller, &request, &response, done);

    std::cout << "TLS RPC sent, waiting for response...\n";
    wg.wait();

    if (controller.Failed())
    {
        std::cerr << "TLS RPC failed: " << controller.ErrorText() << std::endl;
        return 1;
    }

    std::cout << "TLS Response: " << response.message() << std::endl;
    return 0;
}