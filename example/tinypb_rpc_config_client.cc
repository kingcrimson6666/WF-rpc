/**
 * @file tinypb_rpc_config_client.cc
 * @brief TinyPB RPC配置驱动客户端示例
 *
 * 【测试目标】
 * 测试TinyPB RPC的配置驱动功能：使用配置文件驱动所有功能
 *
 * 【测试内容】
 * 1. XML配置文件解析和应用
 * 2. 自动应用服务治理配置（熔断器、限流器）
 * 3. 自动使用配置文件中的负载均衡策略
 * 4. 一行代码获取客户端URL（GetClientUpstreamUrl）
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
#include "tinypb_rpc_channel.h"
#include "tinypb_rpc_controller.h"
#include "rpc_config.h"

static WFFacilities::WaitGroup* wg_ptr = nullptr;

void rpc_callback()
{
    wg_ptr->done();
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <config_file>\n";
        std::cerr << "Example: " << argv[0] << " rpc_config_example.xml\n";
        return 1;
    }

    const std::string config_file = argv[1];

    // 1. 初始化配置文件（自动应用服务治理配置）
    if (wf_rpc::InitConfig(config_file) != 0)
    {
        std::cerr << "Failed to load config file: " << config_file << "\n";
        return 1;
    }

    std::cout << "=== TinyPB RPC Config-Driven Client ===\n";
    std::cout << "Config file: " << config_file << "\n\n";

    // 2. 获取基于配置文件的客户端URL（自动使用upstream或server地址）
    std::string url = wf_rpc::GetClientUpstreamUrl();
    if (url.empty())
    {
        std::cerr << "No upstream or server address configured in config file\n";
        return 1;
    }

    std::cout << "Using client URL: " << url << "\n";

    // 3. 创建RPC通道（自动使用配置文件中的负载均衡策略）
    wf_rpc::TinyPbRpcChannel channel(url);
    wf::rpc::example::EchoService_Stub stub(&channel);

    std::cout << "Features automatically applied:\n";
    std::cout << "  - Service governance (circuit breaker, rate limiter)\n";
    std::cout << "  - Load balancing (upstream configuration)\n";
    std::cout << "  - Monitoring metrics\n";
    std::cout << "  - Structured logging\n\n";

    // 4. 发起RPC调用（自动应用熔断器和限流器）
    WFFacilities::WaitGroup wg(1);
    wg_ptr = &wg;

    wf_rpc::TinyPbRpcController controller;
    controller.SetTimeout(5000);

    wf::rpc::example::EchoRequest request;
    request.set_message("hello_config_rpc");

    wf::rpc::example::EchoResponse response;

    auto done = google::protobuf::NewCallback(rpc_callback);
    stub.Echo(&controller, &request, &response, done);

    std::cout << "RPC request sent, waiting for response...\n";
    wg.wait();

    if (controller.Failed())
    {
        std::cerr << "RPC failed: " << controller.ErrorText() << "\n";
        return 1;
    }

    std::cout << "RPC Success!\n";
    std::cout << "Response: " << response.message() << "\n";

    return 0;
}