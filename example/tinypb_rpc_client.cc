/**
 * @file tinypb_rpc_client.cc
 * @brief TinyPB RPC客户端示例（同步调用）
 *
 * 【测试目标】
 * 测试TinyPB RPC的基础功能：企业级RPC客户端发起同步调用
 *
 * 【测试内容】
 * 1. TinyPB RPC客户端同步调用流程
 * 2. TinyPbRpcChannel和TinyPbRpcController使用
 * 3. Protobuf Service Stub调用
 * 4. 自动集成熔断器、限流器、监控指标
 *
 * 【TinyPB RPC特点】
 * - 基于Protobuf的完整RPC实现
 * - 支持同步和异步调用
 * - 自动集成熔断器、限流器、监控指标
 * - 支持TLS加密通信
 * - 支持请求追踪（msg_req）
 *
 * 【适用场景】
 * - 企业级应用，生产环境部署
 * - 微服务架构，动态服务管理
 * - 需要服务治理和监控的场景
 */

#include <iostream>
#include <workflow/WFFacilities.h>
#include "echo.pb.h"
#include "tinypb_rpc_channel.h"
#include "tinypb_rpc_controller.h"

static WFFacilities::WaitGroup* wg_ptr = nullptr;

void rpc_callback()
{
    wg_ptr->done();
}

int main()
{
    WFFacilities::WaitGroup wg(1);
    wg_ptr = &wg;

    // 创建TinyPB RPC通道
    wf_rpc::TinyPbRpcChannel channel("127.0.0.1", 20001);
    wf::rpc::example::EchoService_Stub stub(&channel);

    // 创建RPC控制器
    wf_rpc::TinyPbRpcController controller;
    controller.SetTimeout(5000);

    // 构造请求
    wf::rpc::example::EchoRequest request;
    request.set_message("hello_tinypb_rpc");

    wf::rpc::example::EchoResponse response;

    // 发起RPC调用（同步）
    auto done = google::protobuf::NewCallback(rpc_callback);
    stub.Echo(&controller, &request, &response, done);

    std::cout << "=== TinyPB RPC Client ===\n";
    std::cout << "Server: 127.0.0.1:20000\n";
    std::cout << "Service: wf.rpc.example.EchoService\n";
    std::cout << "Method: Echo\n";
    std::cout << "Request: hello_tinypb_rpc\n";
    std::cout << "Features:\n";
    std::cout << "  - Automatic circuit breaker\n";
    std::cout << "  - Automatic rate limiter\n";
    std::cout << "  - Automatic monitoring metrics\n";
    std::cout << "  - Request tracing\n\n";

    std::cout << "RPC request sent, waiting for response...\n";
    wg.wait();

    if (controller.Failed())
    {
        std::cerr << "RPC failed: " << controller.ErrorText() << "\n";
        return 1;
    }

    std::cout << "RPC Success!\n";
    std::cout << "Response: " << response.message() << "\n";
    std::cout << "MsgReq: " << controller.GetMsgReq() << "\n";

    return 0;
}