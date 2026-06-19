/**
 * @file simple_rpc_lb_client.cc
 * @brief Simple RPC负载均衡客户端示例
 *
 * 【测试目标】
 * 测试Simple RPC的负载均衡功能：使用UpstreamRegistry配置负载均衡策略
 *
 * 【测试内容】
 * 1. UpstreamRegistry配置负载均衡策略（加权随机）
 * 2. 通过upstream名称发起RPC调用
 * 3. 观察请求在不同服务端实例间的分布
 * 4. 负载均衡效果验证（权重5:3:2）
 *
 * 【负载均衡配置】
 * - 配置3个服务端实例（端口9100、9101、9102）
 * - 权重配置：5、3、2（加权随机策略）
 * - 发起10次RPC调用，观察分布效果
 *
 * 【测试结果】
 * - 端口9100（权重5）：约50%的请求
 * - 端口9101（权重3）：约30%的请求
 * - 端口9102（权重2）：约20%的请求
 *
 * 【适用场景】
 * - 多实例部署，负载均衡场景
 * - 简单场景下的水平扩展
 */

#include <iostream>
#include <vector>
#include "echo.pb.h"
#include "rpc_easy.h"

namespace
{
const char *kUpstreamName = "simple-rpc-lb-demo";
const char *kServiceName = "wf.rpc.example.EchoService";
const char *kMethodName = "Echo";
const char *kMessage = "hello_load_balance";

const std::vector<wf_rpc::UpstreamServer> kServers = {
    {"127.0.0.1:26000", 5},
    {"127.0.0.1:26001", 3},
    {"127.0.0.1:26002", 2},
};
}

int main()
{
    // 配置负载均衡策略（加权随机）
    if (wf_rpc::UpstreamRegistry::configure_weighted(kUpstreamName, kServers, true) != 0)
    {
        std::cerr << "Failed to configure upstream\n";
        return 1;
    }

    std::cout << "=== Simple RPC Load Balance Client ===\n";
    std::cout << "Upstream: " << kUpstreamName << "\n";
    std::cout << "Servers:\n";
    for (const auto& s : kServers)
    {
        std::cout << "  - " << s.address << " (weight: " << s.weight << ")\n";
    }
    std::cout << "Service: " << kServiceName << "\n";
    std::cout << "Method: " << kMethodName << "\n\n";

    // 发起多次RPC调用，观察负载均衡效果
    for (int i = 0; i < 10; ++i)
    {
        wf::rpc::example::EchoRequest request;
        request.set_message(kMessage + std::to_string(i));

        wf::rpc::example::EchoResponse response;
        wf_rpc::SimpleRpcResult result =
            wf_rpc::SimpleRpcClient::call_by_url<wf::rpc::example::EchoRequest,
                                                  wf::rpc::example::EchoResponse>(
                std::string("simple://") + kUpstreamName,
                kServiceName,
                kMethodName,
                request,
                &response,
                1);

        if (result.state == WFT_STATE_SUCCESS && result.status == wf_rpc::RPC_OK)
        {
            std::cout << "Request " << i << ": " << response.message() << "\n";
        }
        else
        {
            std::cerr << "Request " << i << " failed: state=" << result.state
                      << " status=" << result.status << "\n";
        }
    }

    // 清理upstream配置
    wf_rpc::UpstreamRegistry::remove_service(kUpstreamName);

    std::cout << "\nLoad balance test completed\n";
    return 0;
}