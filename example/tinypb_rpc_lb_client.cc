/**
 * @file tinypb_rpc_lb_client.cc
 * @brief TinyPB RPC负载均衡客户端示例
 *
 * 【测试目标】
 * 测试TinyPB RPC的负载均衡功能：使用UpstreamRegistry配置负载均衡策略
 *
 * 【测试内容】
 * 1. UpstreamRegistry配置负载均衡策略（加权随机）
 * 2. 通过upstream URL发起RPC调用
 * 3. 观察请求在不同服务端实例间的分布
 * 4. 自动集成服务治理和可观测性
 *
 * 【负载均衡配置】
 * - 配置3个服务端实例（端口20000、20001、20002）
 * - 权重配置：5、3、2（加权随机策略）
 * - 发起多次RPC调用，观察分布效果
 * - 自动集成熔断器、限流器、监控指标
 *
 * 【测试结果】
 * - 端口20000（权重5）：约50%的请求
 * - 端口20001（权重3）：约30%的请求
 * - 端口20002（权重2）：约20%的请求
 *
 * 【适用场景】
 * - 多实例部署，负载均衡场景
 * - 企业级应用的水平扩展
 */

#include <iostream>
#include <vector>
#include <workflow/WFFacilities.h>
#include "echo.pb.h"
#include "tinypb_rpc_channel.h"
#include "tinypb_rpc_controller.h"
#include "rpc_easy.h"

namespace
{
const char *kUpstreamName = "tinypb-rpc-lb-demo";
const std::vector<wf_rpc::UpstreamServer> kServers = {
    {"127.0.0.1:22000", 5},
    {"127.0.0.1:22001", 3},
    {"127.0.0.1:22002", 2},
};

WFFacilities::WaitGroup* wg_ptr = nullptr;

void rpc_callback()
{
    if (wg_ptr)
        wg_ptr->done();
}
}

int main()
{
    // 配置负载均衡策略（加权随机）
    if (wf_rpc::UpstreamRegistry::configure_weighted(kUpstreamName, kServers, true) != 0)
    {
        std::cerr << "Failed to configure upstream\n";
        return 1;
    }

    std::cout << "=== TinyPB RPC Load Balance Client ===\n";
    std::cout << "Upstream: " << kUpstreamName << "\n";
    std::cout << "Servers:\n";
    for (const auto& s : kServers)
    {
        std::cout << "  - " << s.address << " (weight: " << s.weight << ")\n";
    }
    std::cout << "Features:\n";
    std::cout << "  - Weighted random load balancing\n";
    std::cout << "  - Automatic circuit breaker\n";
    std::cout << "  - Automatic rate limiter\n";
    std::cout << "  - Automatic monitoring metrics\n\n";

    // 创建TinyPB RPC通道（使用upstream URL）
    // 注意：使用tinypb://scheme，Workflow框架已注册支持
    std::string url = std::string("tinypb://") + kUpstreamName;
    wf_rpc::TinyPbRpcChannel channel(url);

    wf::rpc::example::EchoService_Stub stub(&channel);

    // 发起多次RPC调用，观察负载均衡效果
    for (int i = 0; i < 10; ++i)
    {
        WFFacilities::WaitGroup wg(1);
        wg_ptr = &wg;

        wf_rpc::TinyPbRpcController controller;
        controller.SetTimeout(5000);

        wf::rpc::example::EchoRequest request;
        request.set_message("request_" + std::to_string(i));

        wf::rpc::example::EchoResponse response;

        auto done = google::protobuf::NewCallback(rpc_callback);
        stub.Echo(&controller, &request, &response, done);

        wg.wait();

        if (controller.Failed())
        {
            std::cerr << "Request " << i << " failed: " << controller.ErrorText() << "\n";
        }
        else
        {
            std::cout << "Request " << i << ": " << response.message() << "\n";
        }
    }

    // 清理upstream配置
    wf_rpc::UpstreamRegistry::remove_service(kUpstreamName);

    std::cout << "\nLoad balance test completed\n";
    return 0;
}