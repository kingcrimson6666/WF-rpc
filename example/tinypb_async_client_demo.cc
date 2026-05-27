#include <iostream>
#include <workflow/WFFacilities.h>
#include "echo.pb.h"
#include "tinypb_rpc_async_channel.h"
#include "tinypb_rpc_controller.h"

static WFFacilities::WaitGroup* wg_ptr = nullptr;

void async_rpc_callback()
{
    wg_ptr->done();
}

int main()
{
    WFFacilities::WaitGroup wg(1);
    wg_ptr = &wg;

    wf_rpc::TinyPbRpcAsyncChannel channel("127.0.0.1", 20000);
    wf::rpc::example::EchoService_Stub stub(&channel);

    wf_rpc::TinyPbRpcController controller;
    controller.SetTimeout(5000);

    wf::rpc::example::EchoRequest request;
    request.set_message("hello_async_tinypb");

    wf::rpc::example::EchoResponse response;

    auto done = google::protobuf::NewCallback(async_rpc_callback);

    stub.Echo(&controller, &request, &response, done);

    std::cout << "Async RPC sent, waiting for response...\n";
    wg.wait();

    if (controller.Failed())
    {
        std::cerr << "Async RPC failed: " << controller.ErrorText() << "\n";
        return 1;
    }

    std::cout << "Async Response: " << response.message() << "\n";
    return 0;
}