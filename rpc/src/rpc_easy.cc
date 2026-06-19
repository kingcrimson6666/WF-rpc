#include "rpc_easy.h"
#include <workflow/WFGlobal.h>

namespace wf_rpc
{

// 注册Simple RPC的scheme支持
static bool register_simple_scheme()
{
    // 注册simple://scheme，默认端口为9000
    WFGlobal::register_scheme_port("simple", 9000);
    WFGlobal::register_scheme_port("Simple", 9000);
    WFGlobal::register_scheme_port("SIMPLE", 9000);
    
    return true;
}

// 在全局初始化时注册scheme
static bool g_simple_scheme_registered = register_simple_scheme();

EasyRpcServer::EasyRpcServer(const std::string& service_name) :
	service_name_(service_name),
	server_()
{
}

EasyRpcServer::EasyRpcServer(const std::string& service_name,
				 const struct WFServerParams *params) :
	service_name_(service_name),
	server_(params)
{
}

int EasyRpcServer::start(unsigned short port)
{
	return this->server_.start(port);
}

int EasyRpcServer::start(int family, unsigned short port)
{
	return this->server_.start(family, port);
}

int EasyRpcServer::start(const char *host, unsigned short port)
{
	return this->server_.start(host, port);
}

int SimpleRpcServer::run_until_stopped()
{
	if (this->start() != 0)
		return -1;

	this->wait_for_stop();
	this->stop();
	return 0;
}

void SimpleRpcServer::request_stop()
{
	{
		std::lock_guard<std::mutex> lock(this->lifecycle_mutex_);
		this->stop_requested_ = true;
	}
	this->lifecycle_cv_.notify_all();
}

void SimpleRpcServer::wait_for_stop()
{
	std::unique_lock<std::mutex> lock(this->lifecycle_mutex_);
	this->lifecycle_cv_.wait(lock, [this] {
		return this->stop_requested_;
	});
}

void EasyRpcServer::stop()
{
	this->server_.stop();
}

EasyRpcClient::EasyRpcClient(const std::string& host,
				 unsigned short port,
				 const std::string& service_name) :
	host_(host),
	port_(port),
	service_name_(service_name)
{
}

int UpstreamRegistry::create_weighted(const std::string& upstream_name,
				     bool try_another)
{
	return RpcClient::create_weighted_upstream(upstream_name, try_another);
}

int UpstreamRegistry::register_server(const std::string& upstream_name,
				   const std::string& address)
{
	return RpcClient::add_upstream_server(upstream_name, address);
}

int UpstreamRegistry::register_server(const std::string& upstream_name,
				   const std::string& address,
				   const struct AddressParams *params)
{
	return RpcClient::add_upstream_server(upstream_name, address, params);
}

int UpstreamRegistry::unregister_server(const std::string& upstream_name,
				     const std::string& address)
{
	return RpcClient::remove_upstream_server(upstream_name, address);
}

int UpstreamRegistry::remove_service(const std::string& upstream_name)
{
	return RpcClient::delete_upstream(upstream_name);
}

int UpstreamRegistry::configure_weighted(const std::string& upstream_name,
				      const std::vector<UpstreamServer>& servers,
				      bool try_another)
{
	return RpcClient::configure_weighted_upstream(upstream_name,
						      servers,
						      try_another);
}

int UpstreamRegistry::configure_weighted(const std::string& upstream_name,
				      const std::vector<UpstreamServerParams>& servers,
				      bool try_another)
{
	return RpcClient::configure_weighted_upstream(upstream_name,
						      servers,
						      try_another);
}

} // namespace wf_rpc
