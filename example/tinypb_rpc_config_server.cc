/**
 * @file tinypb_rpc_config_server.cc
 * @brief TinyPB RPC配置驱动服务端示例
 *
 * 【测试目标】
 * 测试TinyPB RPC的配置驱动功能：使用配置文件驱动所有功能
 *
 * 【测试内容】
 * 1. XML配置文件解析和应用
 * 2. 自动应用日志配置（级别、路径）
 * 3. 自动应用服务治理配置（熔断器、限流器）
 * 4. 自动应用负载均衡配置
 * 5. 一行代码启动服务（StartRpcServer）
 *
 * 【配置驱动优势】
 * - 无需手动配置，配置文件驱动所有功能
 * - 易于修改和部署
 * - 生产级架构设计
 * - 所有功能自动应用
 *
 * 【运行方式】
 * - 需要提供配置文件（rpc_config_example.xml）
 * - 命令行参数：<config_file>
 *
 * 【适用场景】
 * - 企业级应用，生产环境部署
 * - 配置驱动架构，易于管理
 * - 微服务架构，标准化部署
 */

#include <iostream>
#include <workflow/WFFacilities.h>
#include "echo.pb.h"
#include "tinypb_rpc_server.h"
#include "rpc_config.h"

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
        response->set_message("tinypb_rpc_config: " + request->message());
        if (done)
            done->Run();
    }
};

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <config_file>\n";
        std::cerr << "Example: " << argv[0] << " rpc_config_example.xml\n";
        return 1;
    }

    const std::string config_file = argv[1];

    // 1. 初始化配置文件
    if (wf_rpc::InitConfig(config_file) != 0)
    {
        std::cerr << "Failed to load config file: " << config_file << "\n";
        return 1;
    }

    std::cout << "=== TinyPB RPC Config-Driven Server ===\n";
    std::cout << "Config file: " << config_file << "\n\n";

    // 2. 注册服务
    google::protobuf::Service* service = new EchoServiceImpl();
    wf_rpc::GetRpcServer()->get_dispatcher()->registerService(service);

    // 3. 启动RPC服务器（自动应用所有配置）
    if (wf_rpc::StartRpcServer() != 0)
    {
        std::cerr << "Failed to start RPC server\n";
        return 1;
    }

    std::cout << "RPC server started successfully\n";
    std::cout << "Features automatically applied:\n";
    std::cout << "  - Log configuration (level, path)\n";
    std::cout << "  - Service governance (circuit breaker, rate limiter)\n";
    std::cout << "  - Load balancing (upstream configuration)\n";
    std::cout << "  - Monitoring metrics\n";
    std::cout << "  - Structured logging\n";
    std::cout << "Press Ctrl+C to stop\n\n";

    signal(SIGINT, sig_handler);
    wg.wait();

    wf_rpc::GetRpcServer()->stop();
    std::cout << "Server stopped\n";
    return 0;
}