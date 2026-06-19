/**
 * @file tinypb_rpc_tls_client.cc
 * @brief TinyPB RPC TLS加密客户端示例
 *
 * 【测试目标】
 * 测试TinyPB RPC的TLS加密功能：使用OpenSSL保障传输安全
 *
 * 【测试内容】
 * 1. TinyPB RPC TLS加密客户端调用流程
 * 2. OpenSSL证书加载和验证
 * 3. TLS加密通信验证
 * 4. 安全传输保障
 *
 * 【TLS加密优势】
 * - OpenSSL TLS加密，保障传输安全
 * - 证书验证，防止中间人攻击
 * - 数据加密，防止窃听
 * - 适合安全敏感场景
 *
 * 【运行方式】
 * - 需要提供证书文件（client.crt），可选密钥文件（client.key）
 * - 命令行参数：<host> <port> <cert_file> [key_file]
 *
 * 【适用场景】
 * - 安全敏感场景，需要TLS加密
 * - 企业级应用，生产环境部署
 * - 外网通信，需要数据加密
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

int main(int argc, char* argv[])
{
    if (argc != 4 && argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " <host> <port> <cert_file> [key_file]\n";
        std::cerr << "Example: " << argv[0] << " 127.0.0.1 20000 client.crt\n";
        std::cerr << "Example: " << argv[0] << " 127.0.0.1 20000 client.crt client.key\n";
        return 1;
    }

    const char* host = argv[1];
    unsigned short port = (unsigned short)atoi(argv[2]);
    const std::string cert_file = argv[3];
    std::string key_file;

    if (argc == 5)
    {
        key_file = argv[4];
    }

    WFFacilities::WaitGroup wg(1);
    wg_ptr = &wg;

    // 创建TLS加密的TinyPB RPC通道
    std::unique_ptr<wf_rpc::TinyPbRpcChannel> channel;

    if (key_file.empty())
    {
        channel.reset(new wf_rpc::TinyPbRpcChannel(host, port, cert_file));
    }
    else
    {
        channel.reset(new wf_rpc::TinyPbRpcChannel(host, port, cert_file, key_file));
    }

    wf::rpc::example::EchoService_Stub stub(channel.get());

    // 创建RPC控制器
    wf_rpc::TinyPbRpcController controller;
    controller.SetTimeout(5000);

    // 构造请求
    wf::rpc::example::EchoRequest request;
    request.set_message("hello_tls_rpc");

    wf::rpc::example::EchoResponse response;

    // 发起TLS加密的RPC调用
    auto done = google::protobuf::NewCallback(rpc_callback);
    stub.Echo(&controller, &request, &response, done);

    std::cout << "=== TinyPB RPC TLS Client ===\n";
    std::cout << "Server: " << host << ":" << port << "\n";
    std::cout << "TLS enabled with:\n";
    std::cout << "  - Certificate: " << cert_file << "\n";
    if (!key_file.empty())
    {
        std::cout << "  - Key: " << key_file << "\n";
    }
    std::cout << "Features:\n";
    std::cout << "  - OpenSSL TLS encryption\n";
    std::cout << "  - Certificate verification\n";
    std::cout << "  - Secure data transmission\n\n";

    std::cout << "TLS RPC sent, waiting for response...\n";
    wg.wait();

    if (controller.Failed())
    {
        std::cerr << "TLS RPC failed: " << controller.ErrorText() << "\n";
        return 1;
    }

    std::cout << "TLS RPC Success!\n";
    std::cout << "Response: " << response.message() << "\n";

    return 0;
}