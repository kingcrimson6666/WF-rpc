/**
 * @file tinypb_rpc_async_client.cc
 * @brief TinyPB RPC异步客户端示例
 *
 * 【测试目标】
 * 测试TinyPB RPC的异步调用功能：非阻塞调用，适合高并发场景
 *
 * 【测试内容】
 * 1. TinyPB RPC异步调用流程
 * 2. TinyPbRpcAsyncChannel创建异步通道
 * 3. 通过Closure回调接收响应
 * 4. 自动集成服务治理和可观测性
 *
 * 【异步调用优势】
 * - 非阻塞调用，提高并发性能
 * - 适合高并发场景，减少线程等待
 * - 通过回调机制处理响应
 * - 自动集成熔断器、限流器、监控
 *
 * 【适用场景】
 * - 高并发场景，需要异步调用
 * - 企业级应用，生产环境部署
 * - 微服务架构，动态服务管理
 */

#include <iostream>
#include <workflow/WFFacilities.h>
#include "echo.pb.h"
#include "tinypb_rpc_async_channel.h"
#include "tinypb_rpc_controller.h"

static WFFacilities::WaitGroup* wg_ptr = nullptr;

void async_rpc_callback()
{
    wg_ptr->done();
}

int main()
{
    WFFacilities::WaitGroup wg(1);
    wg_ptr = &wg;

    // 创建TinyPB RPC异步通道
    wf_rpc::TinyPbRpcAsyncChannel channel("127.0.0.1", 20001);
    wf::rpc::example::EchoService_Stub stub(&channel);

    // 创建RPC控制器
    wf_rpc::TinyPbRpcController controller;
    controller.SetTimeout(5000);

    // 构造请求
    wf::rpc::example::EchoRequest request;
    request.set_message("hello_async_tinypb_rpc");

    wf::rpc::example::EchoResponse response;

    // 发起异步RPC调用
    auto done = google::protobuf::NewCallback(async_rpc_callback);
    stub.Echo(&controller, &request, &response, done);

    std::cout << "=== TinyPB RPC Async Client ===\n";
    std::cout << "Server: 127.0.0.1:20001\n";
    std::cout << "Service: wf.rpc.example.EchoService\n";
    std::cout << "Method: Echo\n";
    std::cout << "Request: hello_async_tinypb_rpc\n";
    std::cout << "Features:\n";
    std::cout << "  - Non-blocking async call\n";
    std::cout << "  - Automatic circuit breaker\n";
    std::cout << "  - Automatic rate limiter\n";
    std::cout << "  - Automatic monitoring metrics\n\n";

    std::cout << "Async RPC sent, waiting for response...\n";
    wg.wait();

    if (controller.Failed())
    {
        std::cerr << "Async RPC failed: " << controller.ErrorText() << "\n";
        return 1;
    }

    std::cout << "Async RPC Success!\n";
    std::cout << "Response: " << response.message() << "\n";
    std::cout << "MsgReq: " << controller.GetMsgReq() << "\n";

    return 0;
}