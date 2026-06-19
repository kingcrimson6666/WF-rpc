/**
 * @file simple_rpc_lb_server.cc
 * @brief Simple RPC负载均衡服务端示例
 *
 * 【测试目标】
 * 测试Simple RPC的负载均衡功能：启动多个服务端实例，观察负载均衡效果
 *
 * 【测试内容】
 * 1. 多个Simple RPC服务端实例启动（端口9100、9101、9102）
 * 2. 每个实例响应前缀标识（便于客户端识别）
 * 3. 服务端实例的独立运行和停止
 * 4. 负载均衡场景下的服务端部署
 *
 * 【负载均衡测试】
 * - 配合simple_rpc_lb_client.cc使用
 * - 客户端通过UpstreamRegistry配置负载均衡策略
 * - 观察请求在不同实例间的分布
 *
 * 【适用场景】
 * - 多实例部署，高可用场景
 * - 负载均衡测试和验证
 * - 简单场景下的水平扩展
 */

#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include "echo.pb.h"
#include "rpc_easy.h"

namespace
{
const char *kServerHost = "127.0.0.1";
const char *kServiceName = "wf.rpc.example.EchoService";
const char *kMethodName = "Echo";

volatile std::sig_atomic_t g_stop_flag = 0;

void sig_handler(int)
{
    g_stop_flag = 1;
}
}

int main()
{
    std::signal(SIGINT, sig_handler);
    std::signal(SIGTERM, sig_handler);

    std::vector<unsigned short> ports = {26000, 26001, 26002};
    std::vector<std::unique_ptr<wf_rpc::SimpleRpcServer>> servers;

    // 启动多个服务端实例
    for (unsigned short port : ports)
    {
        auto server = std::unique_ptr<wf_rpc::SimpleRpcServer>(
            new wf_rpc::SimpleRpcServer(kServerHost, port, kServiceName));

        server->register_method<wf::rpc::example::EchoRequest, wf::rpc::example::EchoResponse>(
            kMethodName,
            [port](const wf::rpc::example::EchoRequest& req,
                   wf::rpc::example::EchoResponse& resp) {
                resp.set_message("from_port_" + std::to_string(port) + ": " + req.message());
            });

        if (server->start() != 0)
        {
            std::cerr << "Failed to start server at port " << port << "\n";
            // 回滚已启动的服务端
            for (auto& s : servers)
            {
                s->stop();
            }
            return 1;
        }

        servers.push_back(std::move(server));
    }

    std::cout << "=== Simple RPC Load Balance Server ===\n";
    std::cout << "Started " << ports.size() << " server instances:\n";
    for (unsigned short port : ports)
    {
        std::cout << "  - " << kServerHost << ":" << port << "\n";
    }
    std::cout << "Service: " << kServiceName << "\n";
    std::cout << "Method: " << kMethodName << "\n";
    std::cout << "Press Ctrl+C to stop\n\n";

    // 等待停止信号
    std::thread watcher([&servers]() {
        while (!g_stop_flag)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        for (auto& s : servers)
        {
            s->request_stop();
        }
    });

    for (auto& s : servers)
    {
        s->wait_for_stop();
        s->stop();
    }
    watcher.join();

    std::cout << "All servers stopped\n";
    return 0;
}