/**
 * @file tinypb_rpc_server.cc
 * @brief TinyPB RPC服务端示例
 *
 * 【测试目标】
 * 测试TinyPB RPC的基础功能：企业级RPC服务端启动和Protobuf Service注册
 *
 * 【测试内容】
 * 1. TinyPB RPC服务端启动流程
 * 2. Protobuf Service接口实现和注册
 * 3. 自动集成监控指标和日志系统
 * 4. 服务端停止流程
 *
 * 【TinyPB RPC特点】
 * - 基于Protobuf的完整RPC实现
 * - 支持异步调用、TLS加密、服务治理
 * - 自动集成监控指标和日志系统
 * - 支持服务注册与发现
 * - 数据完整性校验（CRC32）
 *
 * 【适用场景】
 * - 企业级应用，生产环境部署
 * - 微服务架构，动态服务管理
 * - 高并发场景，需要异步调用
 * - 安全敏感场景，需要TLS加密
 */

#include <iostream>
#include <workflow/WFFacilities.h>
#include "echo.pb.h"
#include "tinypb_rpc_server.h"

static WFFacilities::WaitGroup wg(1);

void sig_handler(int signo)
{
    wg.done();
}

// 实现Protobuf Service接口
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

int main()
{
    // 注册服务
    google::protobuf::Service* service = new EchoServiceImpl();
    wf_rpc::GetRpcServer()->get_dispatcher()->registerService(service);

    // 启动服务端
    if (wf_rpc::GetRpcServer()->start("127.0.0.1", 20001) != 0)
    {
        std::cerr << "Failed to start TinyPB RPC server\n";
        return 1;
    }

    std::cout << "=== TinyPB RPC Server ===\n";
    std::cout << "Server started at 127.0.0.1:20001\n";
    std::cout << "Service: wf.rpc.example.EchoService\n";
    std::cout << "Features:\n";
    std::cout << "  - Automatic monitoring metrics\n";
    std::cout << "  - Structured logging\n";
    std::cout << "  - CRC32 data integrity check\n";
    std::cout << "  - Request tracing (msg_req)\n";
    std::cout << "Press Ctrl+C to stop\n\n";

    signal(SIGINT, sig_handler);
    wg.wait();

    wf_rpc::GetRpcServer()->stop();
    std::cout << "Server stopped\n";
    return 0;
}