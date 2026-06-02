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
        response->set_message("echo_registry: " + request->message());
        if (done)
            done->Run();
    }
};

int main(int argc, char* argv[])
{
    if (argc != 3 && argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <port> <service_name> [registry_endpoint]\n";
        std::cerr << "Example: " << argv[0] << " 20000 EchoService\n";
        std::cerr << "Example: " << argv[0] << " 20000 EchoService http://127.0.0.1:2379\n";
        return 1;
    }

    unsigned short port = (unsigned short)atoi(argv[1]);
    const std::string service_name = argv[2];
    std::string registry_endpoint = "127.0.0.1:2379";

    if (argc == 4)
    {
        registry_endpoint = argv[3];
    }

    google::protobuf::Service* service = new EchoServiceImpl();
    wf_rpc::GetRpcServer()->get_dispatcher()->registerService(service);

    wf_rpc::GetRpcServer()->set_registry_enabled(true);
    wf_rpc::GetRpcServer()->set_service_name(service_name);
    wf_rpc::GetRpcServer()->set_registry_endpoint(registry_endpoint);

    std::cout << "Starting RPC server with service discovery:\n";
    std::cout << "  Service name: " << service_name << "\n";
    std::cout << "  Port: " << port << "\n";
    std::cout << "  Registry: " << registry_endpoint << "\n";

    signal(SIGINT, sig_handler);

    if (wf_rpc::GetRpcServer()->start(port) != 0)
    {
        std::cerr << "Failed to start server\n";
        return 1;
    }

    std::cout << "Server is running. Press Ctrl+C to stop.\n";
    wg.wait();

    return 0;
}