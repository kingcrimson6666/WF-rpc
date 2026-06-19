/**
 * @file tinypb_rpc_discovery_server.cc
 * @brief TinyPB RPC服务发现服务端示例
 *
 * 【测试目标】
 * 测试TinyPB RPC的服务发现功能：使用etcd进行服务注册与发现
 *
 * 【测试内容】
 * 1. TinyPB RPC服务端自动注册到etcd
 * 2. etcd租约机制和心跳保活
 * 3. 服务实例管理
 * 4. 动态服务发现支持
 *
 * 【服务发现优势】
 * - 自动服务注册，无需手动配置
 * - etcd租约机制，自动心跳保活
 * - 动态服务发现，客户端自动感知
 * - 适合微服务架构
 *
 * 【运行方式】
 * - 需要etcd服务运行（默认http://127.0.0.1:2379）
 * - 命令行参数：<port> <service_name> [registry_endpoint]
 *
 * 【适用场景】
 * - 微服务架构，动态服务管理
 * - 企业级应用，生产环境部署
 * - 多实例部署，自动服务发现
 */

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
        response->set_message("tinypb_rpc_discovery: " + request->message());
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
    std::string registry_endpoint = "http://127.0.0.1:2379";

    if (argc == 4)
    {
        registry_endpoint = argv[3];
    }

    // 注册服务
    google::protobuf::Service* service = new EchoServiceImpl();
    wf_rpc::GetRpcServer()->get_dispatcher()->registerService(service);

    // 启用服务注册
    wf_rpc::GetRpcServer()->set_registry_enabled(true);
    wf_rpc::GetRpcServer()->set_service_name(service_name);
    wf_rpc::GetRpcServer()->set_registry_endpoint(registry_endpoint);

    // 启动服务端（自动注册到etcd）
    if (wf_rpc::GetRpcServer()->start(port) != 0)
    {
        std::cerr << "Failed to start server\n";
        return 1;
    }

    std::cout << "=== TinyPB RPC Service Discovery Server ===\n";
    std::cout << "Server started at port " << port << "\n";
    std::cout << "Service name: " << service_name << "\n";
    std::cout << "Registry endpoint: " << registry_endpoint << "\n";
    std::cout << "Features:\n";
    std::cout << "  - Automatic service registration\n";
    std::cout << "  - Heartbeat keepalive\n";
    std::cout << "  - Lease management\n";
    std::cout << "  - Automatic monitoring metrics\n";
    std::cout << "Press Ctrl+C to stop\n\n";

    signal(SIGINT, sig_handler);
    wg.wait();

    wf_rpc::GetRpcServer()->stop();
    std::cout << "Server stopped\n";
    return 0;
}