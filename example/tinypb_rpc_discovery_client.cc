/**
 * @file tinypb_rpc_discovery_client.cc
 * @brief TinyPB RPC服务发现客户端示例
 *
 * 【测试目标】
 * 测试TinyPB RPC的服务发现功能：使用etcd进行服务发现和动态负载均衡
 *
 * 【测试内容】
 * 1. TinyPB RPC客户端从etcd发现服务实例
 * 2. 动态负载均衡配置
 * 3. 服务实例变化监听
 * 4. 自动更新负载均衡策略
 *
 * 【服务发现优势】
 * - 自动服务发现，无需手动配置地址
 * - 动态负载均衡，服务变化自动感知
 * - 实时监听服务变化
 * - 适合微服务架构
 *
 * 【运行方式】
 * - 需要etcd服务运行（默认http://127.0.0.1:2379）
 * - 命令行参数：<service_name> <registry_endpoint>
 *
 * 【适用场景】
 * - 微服务架构，动态服务管理
 * - 企业级应用，生产环境部署
 * - 多实例部署，自动服务发现
 */

#include <iostream>
#include <workflow/WFFacilities.h>
#include "echo.pb.h"
#include "rpc_service_registry.h"
#include "tinypb_rpc_channel.h"
#include "tinypb_rpc_controller.h"
#include "rpc_easy.h"

static WFFacilities::WaitGroup wg(1);
static WFFacilities::WaitGroup* rpc_wg_ptr = nullptr;

void sig_handler(int signo)
{
    wg.done();
}

void rpc_callback()
{
    if (rpc_wg_ptr)
        rpc_wg_ptr->done();
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <service_name> <registry_endpoint>\n";
        std::cerr << "Example: " << argv[0] << " EchoService http://127.0.0.1:2379\n";
        return 1;
    }

    const std::string service_name = argv[1];
    const std::string registry_endpoint = argv[2];

    // 设置etcd注册中心地址
    wf_rpc::EtcdRegistryClient::instance().set_endpoint(registry_endpoint);
    wf_rpc::EtcdRegistryClient::instance().start();

    std::cout << "=== TinyPB RPC Service Discovery Client ===\n";
    std::cout << "Service name: " << service_name << "\n";
    std::cout << "Registry endpoint: " << registry_endpoint << "\n\n";

    // 发现服务实例
    auto endpoints = wf_rpc::EtcdRegistryClient::instance().discover(service_name);

    if (endpoints.empty())
    {
        std::cerr << "No endpoints found for service: " << service_name << "\n";
        wf_rpc::EtcdRegistryClient::instance().stop();
        return 1;
    }

    std::cout << "Found " << endpoints.size() << " endpoints:\n";
    for (const auto& ep : endpoints)
    {
        std::cout << "  - " << ep.ip << ":" << ep.port << "\n";
    }
    std::cout << "\n";

    // 配置动态负载均衡
    const std::string upstream_name = "discovery-" + service_name;

    std::vector<wf_rpc::UpstreamServer> servers;
    for (const auto& ep : endpoints)
    {
        servers.push_back({ep.ip + ":" + std::to_string(ep.port), 1});
    }

    wf_rpc::UpstreamRegistry::configure_weighted(upstream_name, servers, true);

    // 监听服务变化，动态更新负载均衡配置
    wf_rpc::EtcdRegistryClient::instance().watch(service_name,
        [upstream_name](const std::vector<wf_rpc::ServiceEndpoint>& endpoints) {
            std::cout << "\n=== Service List Updated ===\n";
            std::cout << "Found " << endpoints.size() << " endpoints:\n";

            std::vector<wf_rpc::UpstreamServer> servers;
            for (const auto& ep : endpoints)
            {
                std::cout << "  - " << ep.ip << ":" << ep.port << "\n";
                servers.push_back({ep.ip + ":" + std::to_string(ep.port), 1});
            }

            if (!servers.empty())
            {
                wf_rpc::UpstreamRegistry::remove_service(upstream_name);
                wf_rpc::UpstreamRegistry::configure_weighted(upstream_name, servers, true);
                std::cout << "Upstream configuration updated\n";
            }
        });

    std::cout << "=== Starting RPC Calls ===\n";
    std::cout << "Features:\n";
    std::cout << "  - Dynamic service discovery\n";
    std::cout << "  - Automatic load balancing\n";
    std::cout << "  - Real-time service updates\n\n";

    signal(SIGINT, sig_handler);

    // 发起RPC调用
    for (int i = 0; i < 5; ++i)
    {
        if (wg.wait(1000) == std::future_status::ready)
        {
            break;
        }

        WFFacilities::WaitGroup rpc_wg(1);
        rpc_wg_ptr = &rpc_wg;

        std::string url = "upstream://" + upstream_name;
        wf_rpc::TinyPbRpcChannel channel(url);

        wf::rpc::example::EchoService_Stub stub(&channel);

        wf_rpc::TinyPbRpcController controller;
        controller.SetTimeout(5000);

        wf::rpc::example::EchoRequest request;
        request.set_message("request_" + std::to_string(i));

        wf::rpc::example::EchoResponse response;

        auto done = google::protobuf::NewCallback(rpc_callback);
        stub.Echo(&controller, &request, &response, done);

        rpc_wg.wait();

        if (controller.Failed())
        {
            std::cerr << "Request " << i << " failed: " << controller.ErrorText() << "\n";
        }
        else
        {
            std::cout << "Request " << i << ": " << response.message() << "\n";
        }
    }

    // 清理资源
    wf_rpc::UpstreamRegistry::remove_service(upstream_name);
    wf_rpc::EtcdRegistryClient::instance().stop();

    std::cout << "\nService discovery test completed\n";
    return 0;
}