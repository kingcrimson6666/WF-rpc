#include <iostream>
#include <workflow/WFFacilities.h>
#include "echo.pb.h"
#include "rpc_service_registry.h"

static WFFacilities::WaitGroup wg(1);

int main(int argc, char* argv[])
{
    if (argc != 3 && argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <service_name> <registry_endpoint> [count]\n";
        std::cerr << "Example: " << argv[0] << " EchoService http://127.0.0.1:2379\n";
        return 1;
    }

    const std::string service_name = argv[1];
    const std::string registry_endpoint = argv[2];
    int count = 1;
    if (argc == 4)
    {
        count = atoi(argv[3]);
    }

    wf_rpc::EtcdRegistryClient::instance().set_endpoint(registry_endpoint);

    std::cout << "Discovering service: " << service_name << "\n";
    std::cout << "Registry endpoint: " << registry_endpoint << "\n\n";

    auto endpoints = wf_rpc::EtcdRegistryClient::instance().discover(service_name);
    
    if (endpoints.empty())
    {
        std::cerr << "No endpoints found for service: " << service_name << "\n";
        return 1;
    }

    std::cout << "Found " << endpoints.size() << " endpoint(s):\n";
    for (const auto& ep : endpoints)
    {
        std::cout << "  - " << ep.ip << ":" << ep.port << "\n";
    }
    std::cout << "\n";

    std::cout << "Watch for service changes (will update every 5 seconds)...\n\n";
    wf_rpc::EtcdRegistryClient::instance().watch(service_name, 
        [](const std::vector<wf_rpc::ServiceEndpoint>& endpoints) {
            std::cout << "Service list updated! Current endpoints:\n";
            for (const auto& ep : endpoints)
            {
                std::cout << "  - " << ep.ip << ":" << ep.port << "\n";
            }
            std::cout << "\n";
        });

    std::cout << "Press Ctrl+C to exit.\n";
    wg.wait(60000);

    return 0;
}