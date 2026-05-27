#include <iostream>
#include <workflow/WFFacilities.h>
#include "echo.pb.h"
#include "tinypb_rpc_channel.h"
#include "tinypb_rpc_controller.h"

static WFFacilities::WaitGroup* wg_ptr = nullptr;
static bool rpc_failed = false;
static std::string rpc_result;

void rpc_callback()
{
    wg_ptr->done();
}

int main()
{
    WFFacilities::WaitGroup wg(1);
    wg_ptr = &wg;

    wf_rpc::TinyPbRpcChannel channel("127.0.0.1", 20000);
    wf::rpc::example::EchoService_Stub stub(&channel);

    wf_rpc::TinyPbRpcController controller;
    controller.SetTimeout(5000);

    wf::rpc::example::EchoRequest request;
    request.set_message("hello_tinypb");

    wf::rpc::example::EchoResponse response;

    auto done = google::protobuf::NewCallback(rpc_callback);

    stub.Echo(&controller, &request, &response, done);

    wg.wait();

    if (controller.Failed())
    {
        std::cerr << "RPC failed: " << controller.ErrorText() << "\n";
        return 1;
    }

    std::cout << "Response: " << response.message() << "\n";
    return 0;
}