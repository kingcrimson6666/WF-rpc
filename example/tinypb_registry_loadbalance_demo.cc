#include <iostream>
#include <vector>
#include <future>
#include <workflow/WFFacilities.h>
#include "echo.pb.h"
#include "rpc_easy.h"
#include "rpc_service_registry.h"

static WFFacilities::WaitGroup wg(1);

void sig_handler(int signo)
{
    wg.done();
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <service_name> <registry_endpoint>\n";
        std::cerr << "Example: " << argv[0] << " EchoService http://127.0.0.1:2379\n";
        return 1;
    }

    const std::string service_name = argv[1];
    const std::string registry_endpoint = argv[2];

    wf_rpc::EtcdRegistryClient::instance().set_endpoint(registry_endpoint);
    wf_rpc::EtcdRegistryClient::instance().start();

    std::cout << "=== Service Discovery + Load Balance Demo ===\n";
    std::cout << "Service Name: " << service_name << "\n";
    std::cout << "Registry: " << registry_endpoint << "\n\n";

    const std::string upstream_name = "registry-upstream-" + service_name;

    wf_rpc::EtcdRegistryClient::instance().watch(service_name, 
        [upstream_name](const std::vector<wf_rpc::ServiceEndpoint>& endpoints) {
            std::cout << "\n=== Service List Updated ===\n";
            std::cout << "Found " << endpoints.size() << " endpoints:\n";
            
            std::vector<wf_rpc::UpstreamServer> servers;
            for (size_t i = 0; i < endpoints.size(); ++i) {
                const auto& ep = endpoints[i];
                std::cout << "  - " << ep.ip << ":" << ep.port << "\n";
                servers.push_back({ep.ip + ":" + std::to_string(ep.port), 1});
            }

            if (!servers.empty()) {
                wf_rpc::ServiceRegistry::remove_service(upstream_name);
                wf_rpc::ServiceRegistry::configure_weighted(upstream_name, servers, true);
                std::cout << "Upstream configured with load balancing\n";
            }
        });

    auto initial_endpoints = wf_rpc::EtcdRegistryClient::instance().discover(service_name);
    if (!initial_endpoints.empty()) {
        std::vector<wf_rpc::UpstreamServer> servers;
        for (const auto& ep : initial_endpoints) {
            servers.push_back({ep.ip + ":" + std::to_string(ep.port), 1});
        }
        wf_rpc::ServiceRegistry::configure_weighted(upstream_name, servers, true);
        std::cout << "Initial upstream configured\n";
    }

    std::cout << "\n=== Starting RPC Calls with Load Balancing ===\n";
    
    signal(SIGINT, sig_handler);

    for (int i = 0; i < 10; ++i) {
        if (wg.wait(1000) == std::future_status::ready) {
            break;
        }

        wf::rpc::example::EchoRequest request;
        request.set_message("request_" + std::to_string(i));

        wf::rpc::example::EchoResponse response;
        wf_rpc::SimpleRpcResult result =
            wf_rpc::SimpleRpcClient::call_by_url<wf::rpc::example::EchoRequest,
                                                 wf::rpc::example::EchoResponse>(
                std::string("upstream://") + upstream_name,
                "wf.rpc.example.EchoService",
                "Echo",
                request,
                &response,
                1);

        if (result.state == WFT_STATE_SUCCESS && result.status == wf_rpc::RPC_OK) {
            std::cout << "Request " << i << ": " << response.message() << "\n";
        } else {
            std::cerr << "Request " << i << " failed: state=" << result.state
                      << " error=" << result.error
                      << " status=" << result.status << "\n";
        }
    }

    wf_rpc::ServiceRegistry::remove_service(upstream_name);
    wf_rpc::EtcdRegistryClient::instance().stop();

    std::cout << "\nDemo completed!\n";
    return 0;
}