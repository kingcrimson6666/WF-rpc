/**
 * @file simple_rpc_client.cc
 * @brief Simple RPC客户端示例
 *
 * 【测试目标】
 * 测试Simple RPC的基础功能：轻量级RPC客户端发起同步阻塞调用
 *
 * 【测试内容】
 * 1. Simple RPC客户端调用流程（五元组调用）
 * 2. Protobuf请求对象构造
 * 3. 同步阻塞调用（SimpleRpcClient::call）
 * 4. 调用结果检查和错误处理
 *
 * 【Simple RPC特点】
 * - 轻量级RPC框架，同步阻塞调用
 * - 五元组调用：host + port + service + method + request
 * - 无TLS支持，无服务治理，无监控指标
 * - 适合简单场景，快速原型开发
 *
 * 【适用场景】
 * - 简单场景，快速原型开发
 * - 内网通信，无安全要求
 * - 单服务实例，无需负载均衡
 */

#include <iostream>
#include "echo.pb.h"
#include "rpc_easy.h"

namespace
{
const char *kServerHost = "127.0.0.1";
const unsigned short kServerPort = 9000;
const char *kServiceName = "wf.rpc.example.EchoService";
const char *kMethodName = "Echo";
const char *kMessage = "hello_simple_rpc";
}

int main()
{
    // 构造Protobuf请求对象
    wf::rpc::example::EchoRequest request;
    request.set_message(kMessage);

    // 发起Simple RPC调用（同步阻塞）
    wf::rpc::example::EchoResponse response;
    wf_rpc::SimpleRpcResult result =
        wf_rpc::SimpleRpcClient::call<wf::rpc::example::EchoRequest,
                                      wf::rpc::example::EchoResponse>(
            kServerHost,
            kServerPort,
            kServiceName,
            kMethodName,
            request,
            &response,
            1);

    // 检查结果
    std::cout << "=== Simple RPC Client ===\n";
    std::cout << "Server: " << kServerHost << ":" << kServerPort << "\n";
    std::cout << "Service: " << kServiceName << "\n";
    std::cout << "Method: " << kMethodName << "\n";
    std::cout << "Request: " << kMessage << "\n\n";

    if (result.state == WFT_STATE_SUCCESS && result.status == wf_rpc::RPC_OK)
    {
        std::cout << "RPC Success!\n";
        std::cout << "Response: " << response.message() << "\n";
    }
    else
    {
        std::cerr << "RPC Failed!\n";
        std::cerr << "State: " << result.state << "\n";
        std::cerr << "Error: " << result.error << "\n";
        std::cerr << "Status: " << result.status << "\n";
        return 1;
    }

    return 0;
}