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
        response->set_message("echo_tls: " + request->message());
        if (done)
            done->Run();
    }
};

int main(int argc, char* argv[])
{
    if (argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " <host> <port> <cert_file> <key_file>\n";
        return 1;
    }

    const char* host = argv[1];
    unsigned short port = (unsigned short)atoi(argv[2]);
    const std::string cert_file = argv[3];
    const std::string key_file = argv[4];

    google::protobuf::Service* service = new EchoServiceImpl();
    wf_rpc::GetRpcServer()->get_dispatcher()->registerService(service);

    std::cout << "TinyPB TLS server started on " << host << ":" << port << "\n";

    signal(SIGINT, sig_handler);

    if (wf_rpc::GetRpcServer()->start(host, port, cert_file, key_file) != 0)
    {
        std::cerr << "Failed to start TLS server\n";
        return 1;
    }

    wg.wait();

    return 0;
}