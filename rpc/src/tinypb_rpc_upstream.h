#ifndef WF_RPC_TINYPB_RPC_UPSTREAM_H
#define WF_RPC_TINYPB_RPC_UPSTREAM_H

#include <string>
#include <vector>
#include <workflow/UpstreamManager.h>

namespace wf_rpc
{

struct TinyPbUpstreamServer
{
    std::string address;
    unsigned short weight;

    TinyPbUpstreamServer(const std::string& addr, unsigned short w = 1)
        : address(addr), weight(w)
    {
    }
};

struct TinyPbUpstreamServerParams
{
    std::string address;
    struct AddressParams params;

    TinyPbUpstreamServerParams(const std::string& addr, const struct AddressParams& p)
        : address(addr), params(p)
    {
    }
};

class TinyPbRpcUpstreamManager
{
public:
    static int create_weighted_upstream(const std::string& upstream_name, bool try_another = true);

    static int create_consistent_hash_upstream(const std::string& upstream_name,
                                               upstream_route_t consistent_hash = nullptr);

    static int create_vnswrr_upstream(const std::string& upstream_name);

    static int delete_upstream(const std::string& upstream_name);

    static int add_server(const std::string& upstream_name, const std::string& address);

    static int add_server_with_weight(const std::string& upstream_name,
                                      const std::string& address,
                                      unsigned short weight);

    static int add_server_with_params(const std::string& upstream_name,
                                      const std::string& address,
                                      const struct AddressParams* params);

    static int remove_server(const std::string& upstream_name, const std::string& address);

    static int configure_weighted_upstream(const std::string& upstream_name,
                                           const std::vector<TinyPbUpstreamServer>& servers,
                                           bool try_another = true);

    static int configure_weighted_upstream(const std::string& upstream_name,
                                           const std::vector<TinyPbUpstreamServerParams>& servers,
                                           bool try_another = true);

    static std::string get_upstream_url(const std::string& upstream_name);
};

}

#endif