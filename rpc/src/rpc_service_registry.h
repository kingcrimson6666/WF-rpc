#ifndef WF_RPC_ETCD_REGISTRY_CLIENT_H
#define WF_RPC_ETCD_REGISTRY_CLIENT_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <mutex>

namespace wf_rpc
{

struct ServiceEndpoint
{
    std::string ip;
    int port;
    int64_t lease_id;
    
    std::string to_string() const
    {
        return ip + ":" + std::to_string(port);
    }
};

class EtcdRegistryClient
{
public:
    static EtcdRegistryClient& instance();
    
    void set_endpoint(const std::string& etcd_endpoint);
    const std::string& get_endpoint() const { return etcd_endpoint_; }
    
    bool register_service(const std::string& service_name,
                         const std::string& ip,
                         int port,
                         int ttl = 30);
    
    bool unregister_service(const std::string& service_name);
    
    std::vector<ServiceEndpoint> discover(const std::string& service_name);
    
    void watch(const std::string& service_name,
               std::function<void(const std::vector<ServiceEndpoint>&)> callback);
    
    void start();
    void stop();
    
    void enable_auto_heartbeat(bool enable) { auto_heartbeat_ = enable; }
    void set_heartbeat_interval(int seconds) { heartbeat_interval_ = seconds; }

private:
    EtcdRegistryClient();
    ~EtcdRegistryClient();
    EtcdRegistryClient(const EtcdRegistryClient&) = delete;
    EtcdRegistryClient& operator=(const EtcdRegistryClient&) = delete;
    
    int64_t create_lease(int ttl);
    bool keep_alive(int64_t lease_id);
    bool revoke_lease(int64_t lease_id);
    
    std::string get_service_key(const std::string& service_name, const std::string& ip, int port);
    std::string get_prefix(const std::string& service_name);
    
    void heartbeat_loop();
    
    bool put_key(const std::string& key, const std::string& value, int64_t lease_id);
    bool delete_key(const std::string& key);
    std::vector<ServiceEndpoint> get_keys(const std::string& prefix);
    bool watch_keys(const std::string& prefix);

private:
    std::string etcd_endpoint_;
    std::atomic<bool> running_;
    std::atomic<bool> auto_heartbeat_;
    int heartbeat_interval_;
    
    std::atomic<int64_t> current_lease_id_;
    std::unordered_map<std::string, ServiceEndpoint> registered_services_;
    std::mutex mutex_;
    
    std::function<void(const std::vector<ServiceEndpoint>&)> watch_callback_;
    std::string watch_service_name_;
};

}

#endif