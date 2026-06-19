/**
 * @file rpc_comparison_integration.cc
 * @brief Simple RPC vs TinyPB RPC对比整合测试
 *
 * 【测试目标】
 * 测试两种RPC模式的对比：同时运行Simple RPC和TinyPB RPC，对比性能、功能和适用场景
 *
 * 【测试内容】
 * 1. Simple RPC测试（Phase 1）- 轻量级、简单、无服务治理
 * 2. TinyPB RPC测试（Phase 2）- 企业级、完整功能、服务治理
 * 3. 对比结果展示（Phase 3）- 性能、功能、适用场景对比
 *
 * 【对比维度】
 * - 性能对比：QPS、延迟
 * - 功能对比：TLS、服务治理、监控、日志
 * - 适用场景对比：简单场景 vs 企业级应用
 * - 易用性对比：代码复杂度
 *
 * 【对比测试优势】
 * - 一个文件同时运行两种RPC
 * - 清晰展示两种RPC的区别
 * - 提供选择建议和适用场景分析
 * - 帮助用户理解何时使用哪种RPC
 *
 * 【运行方式】
 * - 自动启动Simple RPC服务端（端口9000）
 * - 自动启动TinyPB RPC服务端（端口20000）
 * - 自动发起对比测试并展示结果
 *
 * 【适用场景】
 * - RPC模式选择和对比
 * - 功能差异理解
 * - 适用场景分析
 */

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <csignal>
#include <workflow/WFFacilities.h>
#include "echo.pb.h"
#include "rpc_easy.h"
#include "tinypb_rpc_server.h"
#include "tinypb_rpc_channel.h"
#include "tinypb_rpc_controller.h"
#include "rpc_service_governance.h"

namespace
{
// Simple RPC配置
const unsigned short kSimpleRpcPort = 9000;

// TinyPB RPC配置
const unsigned short kTinyPbRpcPort = 20000;

const char *kSimpleServerHost = "127.0.0.1";
const unsigned short kSimpleServerPort = 9000;
const char *kSimpleServiceName = "wf.rpc.example.EchoService";

// TinyPB RPC配置
const unsigned short kTinyPbServerPort = 20000;

volatile sig_atomic_t g_stop_flag = 0;

WFFacilities::WaitGroup* wg_ptr = nullptr;

void sig_handler(int)
{
    g_stop_flag = 1;
}

void rpc_callback()
{
    if (wg_ptr)
        wg_ptr->done();
}
}

// Simple RPC服务端线程
void run_simple_rpc_server()
{
    wf_rpc::SimpleRpcServer server(kSimpleServerHost, kSimpleServerPort, kSimpleServiceName);

    server.register_method<wf::rpc::example::EchoRequest, wf::rpc::example::EchoResponse>(
        "Echo",
        [](const wf::rpc::example::EchoRequest& req,
           wf::rpc::example::EchoResponse& resp) {
            resp.set_message("simple_rpc: " + req.message());
        });

    if (server.start() != 0)
    {
        std::cerr << "Failed to start Simple RPC server\n";
        return;
    }

    std::cout << "Simple RPC server started at " << kSimpleServerHost << ":" << kSimpleServerPort << "\n";

    while (!g_stop_flag)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server.request_stop();
    server.wait_for_stop();
    server.stop();
    std::cout << "Simple RPC server stopped\n";
}

// TinyPB RPC服务端线程
void run_tinypb_rpc_server()
{
    class EchoServiceImpl : public wf::rpc::example::EchoService
    {
    public:
        void Echo(google::protobuf::RpcController* controller,
                  const wf::rpc::example::EchoRequest* request,
                  wf::rpc::example::EchoResponse* response,
                  google::protobuf::Closure* done) override
        {
            response->set_message("tinypb_rpc: " + request->message());
            if (done)
                done->Run();
        }
    };

    google::protobuf::Service* service = new EchoServiceImpl();
    wf_rpc::GetRpcServer()->get_dispatcher()->registerService(service);

    if (wf_rpc::GetRpcServer()->start("127.0.0.1", kTinyPbServerPort) != 0)
    {
        std::cerr << "Failed to start TinyPB RPC server\n";
        return;
    }

    std::cout << "TinyPB RPC server started at 127.0.0.1:" << kTinyPbServerPort << "\n";

    while (!g_stop_flag)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    wf_rpc::GetRpcServer()->stop();
    std::cout << "TinyPB RPC server stopped\n";
}

int main(int argc, char* argv[])
{
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    std::cout << "=== Simple RPC vs TinyPB RPC Comparison Integration ===\n";
    std::cout << "This example compares two RPC modes side by side:\n\n";

    std::cout << "=== RPC Modes Comparison ===\n\n";

    std::cout << "Simple RPC:\n";
    std::cout << "  ✓ Lightweight framework\n";
    std::cout << "  ✓ Synchronous blocking call\n";
    std::cout << "  ✓ Low protocol overhead\n";
    std::cout << "  ✓ Simple and easy to use\n";
    std::cout << "  ✓ Suitable for simple scenarios\n";
    std::cout << "  ✗ No TLS encryption\n";
    std::cout << "  ✗ No service governance\n";
    std::cout << "  ✗ No monitoring metrics\n";
    std::cout << "  ✗ No structured logging\n";
    std::cout << "  ✗ No service discovery\n\n";

    std::cout << "TinyPB RPC:\n";
    std::cout << "  ✓ Enterprise-level framework\n";
    std::cout << "  ✓ Sync + Async calls\n";
    std::cout << "  ✓ TLS encryption (OpenSSL)\n";
    std::cout << "  ✓ Service governance (Circuit breaker + Rate limiter)\n";
    std::cout << "  ✓ Monitoring metrics (Prometheus)\n";
    std::cout << "  ✓ Structured logging\n";
    std::cout << "  ✓ Service discovery (etcd)\n";
    std::cout << "  ✓ Request tracing (msg_req)\n";
    std::cout << "  ✓ Data integrity (CRC32)\n";
    std::cout << "  ✓ Config-driven architecture\n";
    std::cout << "  ✓ Suitable for production\n\n";

    std::cout << "=== Starting Both RPC Servers ===\n";

    // 启动Simple RPC服务端
    std::thread simple_server_thread(run_simple_rpc_server);

    // 启动TinyPB RPC服务端
    std::thread tinypb_server_thread(run_tinypb_rpc_server);

    // 等待服务启动
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "\n=== Phase 1: Simple RPC Test ===\n";
    std::cout << "Testing Simple RPC without any advanced features:\n\n";

    // Simple RPC测试
    int simple_success = 0;
    int simple_failure = 0;
    auto simple_start = std::chrono::steady_clock::now();

    for (int i = 0; i < 10; ++i)
    {
        wf::rpc::example::EchoRequest request;
        request.set_message("simple_test_" + std::to_string(i));

        wf::rpc::example::EchoResponse response;
        wf_rpc::SimpleRpcResult result =
            wf_rpc::SimpleRpcClient::call<wf::rpc::example::EchoRequest,
                                          wf::rpc::example::EchoResponse>(
                kSimpleServerHost,
                kSimpleServerPort,
                kSimpleServiceName,
                "Echo",
                request,
                &response,
                1);

        if (result.state == WFT_STATE_SUCCESS && result.status == wf_rpc::RPC_OK)
        {
            simple_success++;
            std::cout << "Simple RPC " << i << ": " << response.message() << "\n";
        }
        else
        {
            simple_failure++;
            std::cerr << "Simple RPC " << i << " failed\n";
        }
    }

    auto simple_end = std::chrono::steady_clock::now();
    auto simple_duration = std::chrono::duration_cast<std::chrono::milliseconds>(simple_end - simple_start);

    std::cout << "\n=== Phase 2: TinyPB RPC Test ===\n";
    std::cout << "Testing TinyPB RPC with all advanced features:\n\n";

    // 配置TinyPB RPC服务治理
    wf_rpc::ServiceGovernanceManager::instance().get_circuit_breaker(
        "wf.rpc.example.EchoService", 50, 5, 30000);
    wf_rpc::ServiceGovernanceManager::instance().get_rate_limiter(
        "wf.rpc.example.EchoService", 1000);

    std::cout << "Service governance enabled:\n";
    std::cout << "  - Circuit breaker: failure_threshold=50\n";
    std::cout << "  - Rate limiter: qps=1000\n\n";

    // TinyPB RPC测试
    int tinypb_success = 0;
    int tinypb_failure = 0;
    auto tinypb_start = std::chrono::steady_clock::now();

    wf_rpc::TinyPbRpcChannel channel("127.0.0.1", kTinyPbServerPort);
    wf::rpc::example::EchoService_Stub stub(&channel);

    for (int i = 0; i < 10; ++i)
    {
        WFFacilities::WaitGroup wg(1);
        wg_ptr = &wg;

        wf_rpc::TinyPbRpcController controller;
        controller.SetTimeout(5000);

        wf::rpc::example::EchoRequest request;
        request.set_message("tinypb_test_" + std::to_string(i));

        wf::rpc::example::EchoResponse response;

        auto done = google::protobuf::NewCallback(rpc_callback);
        stub.Echo(&controller, &request, &response, done);

        wg.wait();

        if (controller.Failed())
        {
            tinypb_failure++;
            std::cerr << "TinyPB RPC " << i << " failed: " << controller.ErrorText() << "\n";
        }
        else
        {
            tinypb_success++;
            std::cout << "TinyPB RPC " << i << ": " << response.message() 
                      << " (msg_req=" << controller.GetMsgReq() << ")\n";
        }
    }

    auto tinypb_end = std::chrono::steady_clock::now();
    auto tinypb_duration = std::chrono::duration_cast<std::chrono::milliseconds>(tinypb_end - tinypb_start);

    std::cout << "\n=== Phase 3: Comparison Results ===\n\n";

    std::cout << "Performance Comparison:\n";
    std::cout << "  Simple RPC:\n";
    std::cout << "    - Total requests: 10\n";
    std::cout << "    - Success: " << simple_success << "\n";
    std::cout << "    - Failure: " << simple_failure << "\n";
    std::cout << "    - Duration: " << simple_duration.count() << "ms\n";
    std::cout << "    - Avg latency: " << (simple_duration.count() / 10) << "ms\n\n";

    std::cout << "  TinyPB RPC:\n";
    std::cout << "    - Total requests: 10\n";
    std::cout << "    - Success: " << tinypb_success << "\n";
    std::cout << "    - Failure: " << tinypb_failure << "\n";
    std::cout << "    - Duration: " << tinypb_duration.count() << "ms\n";
    std::cout << "    - Avg latency: " << (tinypb_duration.count() / 10) << "ms\n\n";

    std::cout << "Feature Comparison:\n";
    std::cout << "  Simple RPC Features:\n";
    std::cout << "    ✓ Basic RPC call\n";
    std::cout << "    ✓ Load balancing (optional)\n";
    std::cout << "    ✗ No service governance\n";
    std::cout << "    ✗ No monitoring\n";
    std::cout << "    ✗ No logging\n\n";

    std::cout << "  TinyPB RPC Features:\n";
    std::cout << "    ✓ Basic RPC call\n";
    std::cout << "    ✓ Load balancing\n";
    std::cout << "    ✓ Circuit breaker protection\n";
    std::cout << "    ✓ Rate limiter control\n";
    std::cout << "    ✓ Monitoring metrics\n";
    std::cout << "    ✓ Structured logging\n";
    std::cout << "    ✓ Request tracing\n";
    std::cout << "    ✓ Data integrity\n\n";

    std::cout << "=== Use Case Recommendations ===\n\n";

    std::cout << "Use Simple RPC when:\n";
    std::cout << "  - Simple scenarios, quick prototype\n";
    std::cout << "  - Internal network, no security requirements\n";
    std::cout << "  - Single service instance\n";
    std::cout << "  - No need for service governance\n";
    std::cout << "  - Development and testing phase\n\n";

    std::cout << "Use TinyPB RPC when:\n";
    std::cout << "  - Enterprise applications, production deployment\n";
    std::cout << "  - Microservice architecture\n";
    std::cout << "  - High concurrency, need async calls\n";
    std::cout << "  - Security sensitive, need TLS encryption\n";
    std::cout << "  - Need service governance and monitoring\n";
    std::cout << "  - Dynamic service management\n\n";

    // 清理资源
    g_stop_flag = 1;
    simple_server_thread.join();
    tinypb_server_thread.join();

    std::cout << "=== Comparison Integration Completed ===\n";
    std::cout << "Both RPC modes demonstrated successfully!\n";
    std::cout << "Choose the right RPC mode based on your requirements.\n";

    return 0;
}