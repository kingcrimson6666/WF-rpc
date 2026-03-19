#include "rpc_easy.h"

namespace wf_rpc
{

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

int ServiceRegistry::create_weighted(const std::string& upstream_name,
				     bool try_another)
{
	return RpcClient::create_weighted_upstream(upstream_name, try_another);
}

int ServiceRegistry::register_server(const std::string& upstream_name,
				   const std::string& address)
{
	return RpcClient::add_upstream_server(upstream_name, address);
}

int ServiceRegistry::register_server(const std::string& upstream_name,
				   const std::string& address,
				   const struct AddressParams *params)
{
	return RpcClient::add_upstream_server(upstream_name, address, params);
}

int ServiceRegistry::unregister_server(const std::string& upstream_name,
				     const std::string& address)
{
	return RpcClient::remove_upstream_server(upstream_name, address);
}

int ServiceRegistry::remove_service(const std::string& upstream_name)
{
	return RpcClient::delete_upstream(upstream_name);
}

int ServiceRegistry::configure_weighted(const std::string& upstream_name,
				      const std::vector<UpstreamServer>& servers,
				      bool try_another)
{
	return RpcClient::configure_weighted_upstream(upstream_name,
						      servers,
						      try_another);
}

int ServiceRegistry::configure_weighted(const std::string& upstream_name,
				      const std::vector<UpstreamServerParams>& servers,
				      bool try_another)
{
	return RpcClient::configure_weighted_upstream(upstream_name,
						      servers,
						      try_another);
}

} // namespace wf_rpc
