#include "tinypb_rpc_upstream.h"
#include <errno.h>

namespace wf_rpc
{

int TinyPbRpcUpstreamManager::create_weighted_upstream(const std::string& upstream_name, bool try_another)
{
    return UpstreamManager::upstream_create_weighted_random(upstream_name, try_another);
}

int TinyPbRpcUpstreamManager::create_consistent_hash_upstream(const std::string& upstream_name,
                                                               upstream_route_t consistent_hash)
{
    return UpstreamManager::upstream_create_consistent_hash(upstream_name, consistent_hash);
}

int TinyPbRpcUpstreamManager::create_vnswrr_upstream(const std::string& upstream_name)
{
    return UpstreamManager::upstream_create_vnswrr(upstream_name);
}

int TinyPbRpcUpstreamManager::delete_upstream(const std::string& upstream_name)
{
    return UpstreamManager::upstream_delete(upstream_name);
}

int TinyPbRpcUpstreamManager::add_server(const std::string& upstream_name, const std::string& address)
{
    return UpstreamManager::upstream_add_server(upstream_name, address);
}

int TinyPbRpcUpstreamManager::add_server_with_weight(const std::string& upstream_name,
                                                      const std::string& address,
                                                      unsigned short weight)
{
    struct AddressParams params = ADDRESS_PARAMS_DEFAULT;
    params.weight = weight;
    return UpstreamManager::upstream_add_server(upstream_name, address, &params);
}

int TinyPbRpcUpstreamManager::add_server_with_params(const std::string& upstream_name,
                                                      const std::string& address,
                                                      const struct AddressParams* params)
{
    return UpstreamManager::upstream_add_server(upstream_name, address, params);
}

int TinyPbRpcUpstreamManager::remove_server(const std::string& upstream_name, const std::string& address)
{
    return UpstreamManager::upstream_remove_server(upstream_name, address);
}

int TinyPbRpcUpstreamManager::configure_weighted_upstream(const std::string& upstream_name,
                                                           const std::vector<TinyPbUpstreamServer>& servers,
                                                           bool try_another)
{
    if (servers.empty())
    {
        errno = EINVAL;
        return -1;
    }

    if (create_weighted_upstream(upstream_name, try_another) < 0)
        return -1;

    for (const auto& server : servers)
    {
        if (add_server_with_weight(upstream_name, server.address, server.weight) < 0)
        {
            delete_upstream(upstream_name);
            return -1;
        }
    }

    return 0;
}

int TinyPbRpcUpstreamManager::configure_weighted_upstream(const std::string& upstream_name,
                                                           const std::vector<TinyPbUpstreamServerParams>& servers,
                                                           bool try_another)
{
    if (servers.empty())
    {
        errno = EINVAL;
        return -1;
    }

    if (create_weighted_upstream(upstream_name, try_another) < 0)
        return -1;

    for (const auto& server : servers)
    {
        if (add_server_with_params(upstream_name, server.address, &server.params) < 0)
        {
            delete_upstream(upstream_name);
            return -1;
        }
    }

    return 0;
}

std::string TinyPbRpcUpstreamManager::get_upstream_url(const std::string& upstream_name)
{
    return std::string("upstream://") + upstream_name;
}

}