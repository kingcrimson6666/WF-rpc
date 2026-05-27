#include <iostream>
#include <workflow/WFFacilities.h>
#include "echo.pb.h"
#include "tinypb_rpc_server.h"

static WFFacilities::WaitGroup wg(1);

void sig_handler(int signo)
{
    wg.done();
}

class EchoServiceImpl : public wf::rpc::example::EchoService
{
public:
    void Echo(google::protobuf::RpcController* controller,
              const wf::rpc::example::EchoRequest* request,
              wf::rpc::example::EchoResponse* response,
              google::protobuf::Closure* done) override
    {
        response->set_message("echo_tinypb: " + request->message());
        if (done)
            done->Run();
    }
};

int main()
{
    google::protobuf::Service* service = new EchoServiceImpl();
    wf_rpc::GetRpcServer()->get_dispatcher()->registerService(service);
    
    std::cout << "TinyPB server started on port 20000\n";
    
    signal(SIGINT, sig_handler);
    
    if (wf_rpc::GetRpcServer()->start("127.0.0.1", 20000) != 0)
    {
        std::cerr << "Failed to start server\n";
        return 1;
    }
    
    wg.wait();
    
    return 0;
}