/**
 * @file tinypb_rpc_tls_server.cc
 * @brief TinyPB RPC TLS加密服务端示例
 *
 * 【测试目标】
 * 测试TinyPB RPC的TLS加密功能：使用OpenSSL保障传输安全
 *
 * 【测试内容】
 * 1. TinyPB RPC TLS加密服务端启动流程
 * 2. OpenSSL证书和密钥文件加载
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
 * - 需要提供证书文件（server.crt）和密钥文件（server.key）
 * - 命令行参数：<host> <port> <cert_file> <key_file>
 *
 * 【适用场景】
 * - 安全敏感场景，需要TLS加密
 * - 企业级应用，生产环境部署
 * - 外网通信，需要数据加密
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

class EchoServiceImpl : public wf::rpc::example::EchoService
{
public:
    void Echo(google::protobuf::RpcController* controller,
              const wf::rpc::example::EchoRequest* request,
              wf::rpc::example::EchoResponse* response,
              google::protobuf::Closure* done) override
    {
        response->set_message("tinypb_rpc_tls: " + request->message());
        if (done)
            done->Run();
    }
};

int main(int argc, char* argv[])
{
    if (argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " <host> <port> <cert_file> <key_file>\n";
        std::cerr << "Example: " << argv[0] << " 127.0.0.1 20000 server.crt server.key\n";
        return 1;
    }

    const char* host = argv[1];
    unsigned short port = (unsigned short)atoi(argv[2]);
    const std::string cert_file = argv[3];
    const std::string key_file = argv[4];

    // 注册服务
    google::protobuf::Service* service = new EchoServiceImpl();
    wf_rpc::GetRpcServer()->get_dispatcher()->registerService(service);

    // 启动TLS加密服务端
    if (wf_rpc::GetRpcServer()->start(host, port, cert_file, key_file) != 0)
    {
        std::cerr << "Failed to start TLS server\n";
        return 1;
    }

    std::cout << "=== TinyPB RPC TLS Server ===\n";
    std::cout << "Server started at " << host << ":" << port << "\n";
    std::cout << "TLS enabled with:\n";
    std::cout << "  - Certificate: " << cert_file << "\n";
    std::cout << "  - Key: " << key_file << "\n";
    std::cout << "Features:\n";
    std::cout << "  - OpenSSL TLS encryption\n";
    std::cout << "  - Certificate verification\n";
    std::cout << "  - Secure data transmission\n";
    std::cout << "Press Ctrl+C to stop\n\n";

    signal(SIGINT, sig_handler);
    wg.wait();

    wf_rpc::GetRpcServer()->stop();
    std::cout << "Server stopped\n";
    return 0;
}