/**
 * @file simple_rpc_server.cc
 * @brief Simple RPC服务端示例
 *
 * 【测试目标】
 * 测试Simple RPC的基础功能：轻量级RPC服务端启动和Protobuf方法注册
 *
 * 【测试内容】
 * 1. Simple RPC服务端启动流程
 * 2. Protobuf方法处理器注册（register_method）
 * 3. 同步阻塞调用处理
 * 4. 服务端停止流程
 *
 * 【Simple RPC特点】
 * - 轻量级RPC框架，同步阻塞调用
 * - 协议开销小，适合简单场景
 * - 无TLS支持，无服务治理，无监控指标
 * - 直接注册Protobuf方法处理器
 *
 * 【适用场景】
 * - 简单场景，快速原型开发
 * - 内网通信，无安全要求
 * - 单服务实例，无需负载均衡
 */

#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>
#include "echo.pb.h"
#include "rpc_easy.h"

namespace
{
const char *kServerHost = "127.0.0.1";
const unsigned short kServerPort = 9000;
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

    // 创建Simple RPC服务端
    wf_rpc::SimpleRpcServer server(kServerHost, kServerPort, kServiceName);

    // 注册Protobuf方法处理器
    server.register_method<wf::rpc::example::EchoRequest, wf::rpc::example::EchoResponse>(
        kMethodName,
        [](const wf::rpc::example::EchoRequest& req,
           wf::rpc::example::EchoResponse& resp) {
            resp.set_message("simple_rpc: " + req.message());
        });

    // 启动服务端
    if (server.start() != 0)
    {
        std::cerr << "Failed to start Simple RPC server at " << kServerHost
                  << ":" << kServerPort << "\n";
        return 1;
    }

    std::cout << "=== Simple RPC Server ===\n";
    std::cout << "Server started at " << kServerHost << ":" << kServerPort << "\n";
    std::cout << "Service: " << kServiceName << "\n";
    std::cout << "Method: " << kMethodName << "\n";
    std::cout << "Press Ctrl+C to stop\n\n";

    // 等待停止信号
    std::thread watcher([&server]() {
        while (!g_stop_flag)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        server.request_stop();
    });

    server.wait_for_stop();
    server.stop();
    watcher.join();

    std::cout << "Server stopped\n";
    return 0;
}