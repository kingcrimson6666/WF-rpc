#include <iostream>
#include <vector>
#include <csignal>
#include <thread>
#include <chrono>

#include "echo.pb.h"
#include "rpc_easy.h"

namespace {

const char* kUpstreamName = "tinypb-loadbalance-demo";
const char* kServiceName = "wf.rpc.example.EchoService";
const char* kMethodName = "Echo";

volatile std::sig_atomic_t g_stop_flag = 0;

void sig_handler(int) {
    g_stop_flag = 1;
}

} // namespace

void echo_handler(const wf::rpc::example::EchoRequest& request,
                  wf::rpc::example::EchoResponse& response) {
    response.set_message("echo_from_server: " + request.message());
}

void run_server(unsigned short port) {
    wf_rpc::SimpleRpcServer server("127.0.0.1", port, kServiceName);
    server.register_method<wf::rpc::example::EchoRequest,
                          wf::rpc::example::EchoResponse>(kMethodName, echo_handler);
    
    char host[32];
    snprintf(host, sizeof(host), "127.0.0.1:%d", port);
    std::cout << "Server starting on " << host << std::endl;
    
    if (server.start() != 0) {
        std::cerr << "Failed to start server on port " << port << std::endl;
        return;
    }
    
    while (!g_stop_flag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    server.stop();
}

int main() {
    std::signal(SIGINT, sig_handler);
    
    std::vector<std::thread> server_threads;
    std::vector<unsigned short> ports = {20000, 20001, 20002};
    
    for (unsigned short port : ports) {
        server_threads.emplace_back(run_server, port);
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    std::vector<wf_rpc::UpstreamServer> servers = {
        {"127.0.0.1:20000", 5},
        {"127.0.0.1:20001", 3},
        {"127.0.0.1:20002", 2},
    };
    
    if (wf_rpc::ServiceRegistry::configure_weighted(kUpstreamName, servers, true) != 0) {
        std::cerr << "Failed to configure upstream" << std::endl;
        return 1;
    }
    
    std::cout << "\n=== Load Balance Test ===" << std::endl;
    std::cout << "Upstream: " << kUpstreamName << std::endl;
    std::cout << "Servers: " << std::endl;
    for (const auto& s : servers) {
        std::cout << "  - " << s.address << " (weight: " << s.weight << ")" << std::endl;
    }
    
    const int num_requests = 10;
    std::cout << "\nSending " << num_requests << " requests..." << std::endl;
    
    for (int i = 0; i < num_requests; ++i) {
        wf::rpc::example::EchoRequest request;
        request.set_message("request_" + std::to_string(i));
        
        wf::rpc::example::EchoResponse response;
        wf_rpc::SimpleRpcResult result =
            wf_rpc::SimpleRpcClient::call<wf::rpc::example::EchoRequest,
                                          wf::rpc::example::EchoResponse>(
                kUpstreamName,
                20000,
                kServiceName,
                kMethodName,
                request,
                &response,
                1);
        
        if (result.state == WFT_STATE_SUCCESS && result.status == wf_rpc::RPC_OK) {
            std::cout << "Request " << i << ": " << response.message() << std::endl;
        } else {
            std::cerr << "Request " << i << " failed: state=" << result.state
                      << " error=" << result.error
                      << " status=" << result.status << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    wf_rpc::ServiceRegistry::remove_service(kUpstreamName);
    
    g_stop_flag = 1;
    for (auto& t : server_threads) {
        if (t.joinable()) t.join();
    }
    
    std::cout << "\nTest completed!" << std::endl;
    return 0;
}