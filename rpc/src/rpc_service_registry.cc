#include "rpc_service_registry.h"
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <unistd.h>
#include <mutex>
#include <condition_variable>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <vector>

namespace wf_rpc
{

static const char* base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static std::string base64_encode(const std::string& input)
{
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    int in_len = (int)input.size();
    const unsigned char* bytes_to_encode = (const unsigned char*)input.c_str();
    
    while (in_len--)
    {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3)
        {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for(i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    
    if (i)
    {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        
        for (j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];
        
        while((i++ < 3))
            ret += '=';
    }
    
    return ret;
}

static std::string base64_decode(const std::string& input)
{
    std::string ret;
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::vector<unsigned char> v;
    
    for (unsigned char c : input)
    {
        if (c == '=') break;
        v.push_back(c);
    }
    
    int in_len = (int)v.size();
    
    while (in_len--)
    {
        char_array_4[i++] = v[in_++];
        if (i == 4)
        {
            for(i = 0; i < 4; i++)
                char_array_4[i] = (unsigned char)(strchr(base64_chars, char_array_4[i]) - base64_chars);
            
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            
            for(i = 0; i < 3; i++)
                ret += char_array_3[i];
            i = 0;
        }
    }
    
    if (i)
    {
        for(j = 0; j < i; j++)
            char_array_4[j] = (unsigned char)(strchr(base64_chars, char_array_4[j]) - base64_chars);
        
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        
        for (j = 0; j < i - 1; j++)
            ret += char_array_3[j];
    }
    
    return ret;
}

static int create_tcp_socket(const std::string& host, int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        return -1;
    }

    struct hostent* server = gethostbyname(host.c_str());
    if (server == NULL)
    {
        close(sock);
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        close(sock);
        return -1;
    }

    return sock;
}

static std::string build_etcd_url(const std::string& endpoint, const std::string& path)
{
    if (endpoint.find("http://") == 0)
    {
        return endpoint + path;
    }
    else if (endpoint.find("https://") == 0)
    {
        return endpoint + path;
    }
    return "http://" + endpoint + path;
}

static void parse_endpoint(const std::string& endpoint, std::string& host, int& port)
{
    std::string addr = endpoint;
    if (addr.find("http://") == 0)
    {
        addr = addr.substr(7);
    }
    else if (addr.find("https://") == 0)
    {
        addr = addr.substr(8);
    }
    
    size_t pos = addr.find(':');
    if (pos != std::string::npos)
    {
        host = addr.substr(0, pos);
        port = std::atoi(addr.substr(pos + 1).c_str());
    }
    else
    {
        host = addr;
        port = 2379;
    }
}

static bool send_http_request(const std::string& endpoint, const std::string& path, 
                              const std::string& body, std::string& response)
{
    std::string host;
    int port;
    parse_endpoint(endpoint, host, port);
    
    int sock = create_tcp_socket(host, port);
    if (sock < 0)
    {
        std::cerr << "Failed to connect to etcd: " << host << ":" << port << std::endl;
        return false;
    }

    std::ostringstream oss;
    oss << "POST " << path << " HTTP/1.1\r\n";
    oss << "Host: " << host << ":" << port << "\r\n";
    oss << "Content-Type: application/json\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;

    std::string request = oss.str();
    ssize_t sent = send(sock, request.c_str(), request.size(), 0);
    if (sent != (ssize_t)request.size())
    {
        close(sock);
        return false;
    }

    char buffer[8192];
    std::string resp;
    ssize_t received;
    while ((received = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[received] = '\0';
        resp += buffer;
    }
    close(sock);

    size_t header_end = resp.find("\r\n\r\n");
    if (header_end == std::string::npos)
    {
        return false;
    }

    size_t body_start = header_end + 4;
    response = resp.substr(body_start);
    
    size_t pos = response.find("\r\n");
    if (pos != std::string::npos)
    {
        response = response.substr(0, pos);
    }

    return true;
}

static std::string etcd_v3_lease_grant_json(int ttl)
{
    std::ostringstream oss;
    oss << "{\"ID\":0,\"TTL\":" << ttl << "}";
    return oss.str();
}

static std::string etcd_v3_lease_keepalive_json(int64_t lease_id)
{
    std::ostringstream oss;
    oss << "{\"ID\":" << lease_id << "}";
    return oss.str();
}

static std::string etcd_v3_lease_revoke_json(int64_t lease_id)
{
    std::ostringstream oss;
    oss << "{\"ID\":" << lease_id << "}";
    return oss.str();
}

static std::string etcd_v3_put_json(const std::string& key, const std::string& value, int64_t lease_id)
{
    std::ostringstream oss;
    std::string encoded_key = base64_encode(key);
    std::string encoded_value = base64_encode(value);
    oss << "{\"key\":\"" << encoded_key << "\"";
    oss << ",\"value\":\"" << encoded_value << "\"";
    if (lease_id > 0)
    {
        oss << ",\"lease\":\"" << lease_id << "\"";
    }
    oss << "}";
    return oss.str();
}

static std::string etcd_v3_range_json(const std::string& key_prefix)
{
    std::ostringstream oss;
    std::string encoded_key = base64_encode(key_prefix);
    
    // Set range_end to a key that's one byte larger, prefix range matching
    std::string range_end_str = key_prefix;
    if (!range_end_str.empty()) {
        // Find the first position to increment
        bool found = false;
        for (int i = range_end_str.size() - 1; i >= 0; --i) {
            unsigned char ch = (unsigned char)range_end_str[i];
            if (ch < 0xFF) {
                range_end_str[i] = (char)(ch + 1);
                range_end_str.resize(i + 1);
                found = true;
                break;
            }
        }
        if (!found) {
            range_end_str += '\x01';  // If all are 0xFF, just add a byte
        }
    } else {
        range_end_str = "\x00";
    }
    std::string encoded_range_end = base64_encode(range_end_str);
    
    oss << "{\"key\":\"" << encoded_key << "\",\"range_end\":\"" << encoded_range_end << "\"}";
    return oss.str();
}

static std::string etcd_v3_delete_json(const std::string& key)
{
    std::ostringstream oss;
    std::string encoded_key = base64_encode(key);
    oss << "{\"key\":\"" << encoded_key << "\"}";
    return oss.str();
}

EtcdRegistryClient& EtcdRegistryClient::instance()
{
    static EtcdRegistryClient instance;
    return instance;
}

EtcdRegistryClient::EtcdRegistryClient()
    : etcd_endpoint_("127.0.0.1:2379"),
      running_(false),
      auto_heartbeat_(true),
      heartbeat_interval_(10),
      current_lease_id_(0)
{
}

EtcdRegistryClient::~EtcdRegistryClient()
{
    stop();
}

void EtcdRegistryClient::set_endpoint(const std::string& etcd_endpoint)
{
    etcd_endpoint_ = etcd_endpoint;
}

bool EtcdRegistryClient::register_service(const std::string& service_name,
                                     const std::string& ip,
                                     int port,
                                     int ttl)
{
    int64_t lease_id = create_lease(ttl);
    if (lease_id <= 0)
    {
        std::cerr << "Failed to create lease for service registration" << std::endl;
        return false;
    }
    
    std::string key = get_service_key(service_name, ip, port);
    std::string value = ip + ":" + std::to_string(port);
    
    if (!put_key(key, value, lease_id))
    {
        std::cerr << "Failed to put service key to etcd" << std::endl;
        revoke_lease(lease_id);
        return false;
    }
    
    ServiceEndpoint endpoint;
    endpoint.ip = ip;
    endpoint.port = port;
    endpoint.lease_id = lease_id;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        registered_services_[key] = endpoint;
    }
    
    std::cout << "Service registered: " << service_name << " -> " << ip << ":" << port << std::endl;
    return true;
}

bool EtcdRegistryClient::unregister_service(const std::string& service_name)
{
    std::string prefix = get_prefix(service_name);
    auto endpoints = get_keys(prefix);
    
    for (const auto& ep : endpoints)
    {
        std::string key = get_service_key(service_name, ep.ip, ep.port);
        if (!delete_key(key))
        {
            std::cerr << "Failed to delete service key: " << key << std::endl;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        registered_services_.erase(key);
    }
    
    std::cout << "Service unregistered: " << service_name << std::endl;
    return true;
}

std::vector<ServiceEndpoint> EtcdRegistryClient::discover(const std::string& service_name)
{
    std::string prefix = get_prefix(service_name);
    return get_keys(prefix);
}

void EtcdRegistryClient::watch(const std::string& service_name,
                           std::function<void(const std::vector<ServiceEndpoint>&)> callback)
{
    watch_service_name_ = service_name;
    watch_callback_ = callback;
    watch_keys(get_prefix(service_name));
}

void EtcdRegistryClient::start()
{
    if (running_.load())
    {
        return;
    }
    
    running_ = true;
    
    if (auto_heartbeat_.load())
    {
        std::thread([this]() { heartbeat_loop(); }).detach();
    }
    
    std::cout << "ServiceRegistry started with endpoint: " << etcd_endpoint_ << std::endl;
}

void EtcdRegistryClient::stop()
{
    if (!running_.load())
    {
        return;
    }
    
    running_ = false;
    
    std::vector<int64_t> lease_ids;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& pair : registered_services_)
        {
            lease_ids.push_back(pair.second.lease_id);
        }
        registered_services_.clear();
    }
    
    for (int64_t lease_id : lease_ids)
    {
        revoke_lease(lease_id);
    }
    
    std::cout << "ServiceRegistry stopped" << std::endl;
}

int64_t EtcdRegistryClient::create_lease(int ttl)
{
    std::string response;
    std::string body = etcd_v3_lease_grant_json(ttl);
    
    if (!send_http_request(etcd_endpoint_, "/v3/lease/grant", body, response))
    {
        std::cerr << "Failed to send lease grant request" << std::endl;
        return -1;
    }
    
    size_t id_pos = response.find("\"ID\":\"");
    if (id_pos != std::string::npos)
    {
        id_pos += 6;
        std::string num_str;
        for (size_t i = id_pos; i < response.length() && i < id_pos + 30; i++)
        {
            char c = response[i];
            if (c >= '0' && c <= '9')
            {
                num_str += c;
            }
            else
            {
                break;
            }
        }
        if (!num_str.empty())
        {
            current_lease_id_ = std::stoll(num_str);
            return current_lease_id_;
        }
    }
    
    std::cerr << "Failed to parse lease ID from response: " << response << std::endl;
    return -1;
}

bool EtcdRegistryClient::keep_alive(int64_t lease_id)
{
    std::string response;
    std::string body = etcd_v3_lease_keepalive_json(lease_id);
    
    return send_http_request(etcd_endpoint_, "/v3/lease/keepalive", body, response);
}

bool EtcdRegistryClient::revoke_lease(int64_t lease_id)
{
    std::string response;
    std::string body = etcd_v3_lease_revoke_json(lease_id);
    
    return send_http_request(etcd_endpoint_, "/v3/lease/revoke", body, response);
}

bool EtcdRegistryClient::put_key(const std::string& key, const std::string& value, int64_t lease_id)
{
    std::string response;
    std::string body = etcd_v3_put_json(key, value, lease_id);
    
    if (!send_http_request(etcd_endpoint_, "/v3/kv/put", body, response))
    {
        return false;
    }
    
    return response.find("\"error\"") == std::string::npos;
}

bool EtcdRegistryClient::delete_key(const std::string& key)
{
    std::string response;
    std::string body = etcd_v3_delete_json(key);
    
    return send_http_request(etcd_endpoint_, "/v3/kv/deleterange", body, response);
}

std::vector<ServiceEndpoint> EtcdRegistryClient::get_keys(const std::string& prefix)
{
    std::vector<ServiceEndpoint> endpoints;
    std::string response;
    std::string body = etcd_v3_range_json(prefix);
    
    if (!send_http_request(etcd_endpoint_, "/v3/kv/range", body, response))
    {
        std::cerr << "Failed to send range request for prefix: " << prefix << std::endl;
        return endpoints;
    }
    
    size_t kvs_pos = response.find("\"kvs\"");
    if (kvs_pos == std::string::npos)
    {
        return endpoints;
    }
    
    size_t value_pos = response.find("\"value\":\"", kvs_pos);
    while (value_pos != std::string::npos)
    {
        size_t value_start = value_pos + 9;
        size_t value_end = response.find("\"", value_start);
        if (value_end != std::string::npos)
        {
            std::string encoded_value = response.substr(value_start, value_end - value_start);
            std::string decoded_value = base64_decode(encoded_value);
            
            size_t colon_pos = decoded_value.find(':');
            if (colon_pos != std::string::npos)
            {
                ServiceEndpoint ep;
                ep.ip = decoded_value.substr(0, colon_pos);
                ep.port = std::atoi(decoded_value.substr(colon_pos + 1).c_str());
                ep.lease_id = 0;
                
                endpoints.push_back(ep);
            }
        }
        value_pos = response.find("\"value\":\"", value_pos + 1);
    }
    
    return endpoints;
}

bool EtcdRegistryClient::watch_keys(const std::string& prefix)
{
    if (!watch_callback_)
    {
        return false;
    }
    
    std::thread([this, prefix]() {
        while (running_.load())
        {
            auto endpoints = get_keys(prefix);
            watch_callback_(endpoints);
            sleep(5);
        }
    }).detach();
    
    return true;
}

void EtcdRegistryClient::heartbeat_loop()
{
    while (running_.load())
    {
        sleep(heartbeat_interval_);
        
        if (!running_.load())
        {
            break;
        }
        
        std::vector<int64_t> lease_ids;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& pair : registered_services_)
            {
                lease_ids.push_back(pair.second.lease_id);
            }
        }
        
        for (int64_t lease_id : lease_ids)
        {
            if (!running_.load())
                break;
            if (!keep_alive(lease_id))
            {
                std::cerr << "Failed to keep alive lease: " << lease_id << std::endl;
            }
        }
    }
}

std::string EtcdRegistryClient::get_service_key(const std::string& service_name, const std::string& ip, int port)
{
    std::string key = "/wf_rpc/services/" + service_name + "/" + ip + ":" + std::to_string(port);
    return key;
}

std::string EtcdRegistryClient::get_prefix(const std::string& service_name)
{
    return "/wf_rpc/services/" + service_name + "/";
}

}
