/**
 * @file tinypb_rpc_lb_server.cc
 * @brief TinyPB RPC负载均衡服务端示例
 *
 * 【测试目标】
 * 测试TinyPB RPC的负载均衡功能：启动多个服务端实例，观察负载均衡效果
 *
 * 【测试内容】
 * 1. 多个TinyPB RPC服务端实例启动（端口20000、20001、20002）
 * 2. 每个实例响应前缀标识（便于客户端识别）
 * 3. 自动集成监控指标和日志系统
 * 4. 负载均衡场景下的服务端部署
 *
 * 【负载均衡测试】
 * - 配合tinypb_rpc_lb_client.cc使用
 * - 客户端通过UpstreamRegistry配置负载均衡策略
 * - 观察请求在不同实例间的分布
 * - 自动集成熔断器、限流器、监控
 *
 * 【适用场景】
 * - 多实例部署，高可用场景
 * - 负载均衡测试和验证
 * - 企业级应用的水平扩展
 */

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <csignal>
#include <workflow/WFFacilities.h>
#include "echo.pb.h"
#include "tinypb_rpc_server.h"

namespace
{
std::vector<unsigned short> ports = {22000, 22001, 22002};
volatile sig_atomic_t g_stop_flag = 0;

void sig_handler(int)
{
    g_stop_flag = 1;
}
}

class EchoServiceImpl : public wf::rpc::example::EchoService
{
private:
    unsigned short port_;

public:
    EchoServiceImpl(unsigned short port) : port_(port) {}

    void Echo(google::protobuf::RpcController* controller,
              const wf::rpc::example::EchoRequest* request,
              wf::rpc::example::EchoResponse* response,
              google::protobuf::Closure* done) override
    {
        response->set_message("from_port_" + std::to_string(port_) + ": " + request->message());
        if (done)
            done->Run();
    }
};

void run_server(unsigned short port)
{
    // 为每个端口创建独立的TinyPbRpcServer实例
    wf_rpc::TinyPbRpcServer server;
    
    google::protobuf::Service* service = new EchoServiceImpl(port);
    server.get_dispatcher()->registerService(service);

    if (server.start("127.0.0.1", port) != 0)
    {
        std::cerr << "Failed to start server at port " << port << "\n";
        delete service;
        return;
    }

    std::cout << "Server started at 127.0.0.1:" << port << "\n";

    while (!g_stop_flag)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server.stop();
    std::cout << "Server at port " << port << " stopped\n";
    delete service;
}

int main()
{
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    std::cout << "=== TinyPB RPC Load Balance Server ===\n";
    std::cout << "Starting " << ports.size() << " server instances:\n";
    for (unsigned short port : ports)
    {
        std::cout << "  - 127.0.0.1:" << port << "\n";
    }
    std::cout << "Features:\n";
    std::cout << "  - Automatic monitoring metrics\n";
    std::cout << "  - Structured logging\n";
    std::cout << "  - Request tracing\n\n";

    // 启动多个服务端线程
    std::vector<std::thread> server_threads;
    for (unsigned short port : ports)
    {
        server_threads.emplace_back(run_server, port);
    }

    // 等待所有线程结束
    for (auto& t : server_threads)
    {
        t.join();
    }

    std::cout << "All servers stopped\n";
    return 0;
}