#include "rpc_framework.h"

#include <errno.h>
#include <utility>

namespace wf_rpc
{

RpcServer::RpcServer() :
	server_(new RpcWfServer(std::bind(&RpcServer::on_process,
								 this,
								 std::placeholders::_1)))
{
}

RpcServer::RpcServer(const struct WFServerParams *params) :
	server_(new RpcWfServer(params,
						std::bind(&RpcServer::on_process,
								 this,
								 std::placeholders::_1)))
{
}

RpcServer::~RpcServer()
{
}

int RpcServer::start(unsigned short port)
{
	return this->server_->start(port);
}

int RpcServer::start(int family, unsigned short port)
{
	return this->server_->start(family, port);
}

int RpcServer::start(const char *host, unsigned short port)
{
	return this->server_->start(host, port);
}

void RpcServer::stop()
{
	this->server_->stop();
}

std::string RpcServer::key_of(const std::string& service, const std::string& method)
{
	return service + "#" + method;
}

void RpcServer::register_binary_method(const std::string& service,
								const std::string& method,
								binary_handler_t handler)
{
	std::lock_guard<std::mutex> lock(this->mutex_);
	this->handlers_[key_of(service, method)] = std::move(handler);
}

void RpcServer::on_process(RpcTask *task)
{
	RpcRequest *req = task->get_req();
	RpcResponse *resp = task->get_resp();
	resp->set_sequence(req->get_sequence());
	resp->set_flags(0);

	std::string service;
	std::string method;
	if (!req->get_service_method(&service, &method))
	{
		resp->set_status(RPC_BAD_REQUEST);
		resp->set_payload("invalid rpc meta");
		return;
	}

	binary_handler_t handler;
	{
		std::lock_guard<std::mutex> lock(this->mutex_);
		auto it = this->handlers_.find(key_of(service, method));
		if (it == this->handlers_.end())
		{
			resp->set_status(RPC_NOT_FOUND);
			resp->set_payload("service or method not found");
			return;
		}
		handler = it->second;
	}

	std::string out;
	uint32_t status = handler(req->payload(), &out);
	resp->set_status(status);
	if (status == RPC_OK)
		resp->set_payload(std::move(out));
}

RpcTask *RpcClient::create_task(const std::string& host,
						 unsigned short port,
						 int retry_max,
						 rpc_callback_t callback)
{
	using Factory = WFNetworkTaskFactory<RpcRequest, RpcResponse>;
	RpcTask *task = Factory::create_client_task(TT_TCP,
									 host,
									 port,
									 retry_max,
									 std::move(callback));
	task->set_keep_alive(30 * 1000);
	return task;
}

RpcTask *RpcClient::create_task_by_url(const std::string& url,
								int retry_max,
								rpc_callback_t callback)
{
	using Factory = WFNetworkTaskFactory<RpcRequest, RpcResponse>;
	RpcTask *task = Factory::create_client_task(TT_TCP,
									 url.c_str(),
									 retry_max,
									 std::move(callback));
	task->set_keep_alive(30 * 1000);
	return task;
}

int RpcClient::create_weighted_upstream(const std::string& upstream_name,
							bool try_another)
{
	return UpstreamManager::upstream_create_weighted_random(upstream_name,
											try_another);
}

int RpcClient::create_consistent_hash_upstream(const std::string& upstream_name,
							   upstream_route_t consistent_hash)
{
	return UpstreamManager::upstream_create_consistent_hash(upstream_name,
											consistent_hash);
}

int RpcClient::create_manual_upstream(const std::string& upstream_name,
					  upstream_route_t select,
					  bool try_another,
					  upstream_route_t consistent_hash)
{
	return UpstreamManager::upstream_create_manual(upstream_name,
								   select,
								   try_another,
								   consistent_hash);
}

int RpcClient::create_vnswrr_upstream(const std::string& upstream_name)
{
	return UpstreamManager::upstream_create_vnswrr(upstream_name);
}

int RpcClient::delete_upstream(const std::string& upstream_name)
{
	return UpstreamManager::upstream_delete(upstream_name);
}

int RpcClient::add_upstream_server(const std::string& upstream_name,
						const std::string& address)
{
	return UpstreamManager::upstream_add_server(upstream_name, address);
}

int RpcClient::add_upstream_server(const std::string& upstream_name,
						const std::string& address,
						const struct AddressParams *params)
{
	return UpstreamManager::upstream_add_server(upstream_name, address, params);
}

int RpcClient::remove_upstream_server(const std::string& upstream_name,
						   const std::string& address)
{
	return UpstreamManager::upstream_remove_server(upstream_name, address);
}

int RpcClient::configure_weighted_upstream(const std::string& upstream_name,
									const std::vector<UpstreamServer>& servers,
									bool try_another)
{
	if (servers.empty())
	{
		errno = EINVAL;
		return -1;
	}

	if (create_weighted_upstream(upstream_name, try_another) < 0)
		return -1;

	for (size_t i = 0; i < servers.size(); i++)
	{
		struct AddressParams params = ADDRESS_PARAMS_DEFAULT;
		params.weight = servers[i].weight ? servers[i].weight : 1;
		if (add_upstream_server(upstream_name, servers[i].address, &params) < 0)
		{
			delete_upstream(upstream_name);
			return -1;
		}
	}

	return 0;
}

int RpcClient::configure_weighted_upstream(const std::string& upstream_name,
									const std::vector<UpstreamServerParams>& servers,
									bool try_another)
{
	if (servers.empty())
	{
		errno = EINVAL;
		return -1;
	}

	if (create_weighted_upstream(upstream_name, try_another) < 0)
		return -1;

	for (size_t i = 0; i < servers.size(); i++)
	{
		if (add_upstream_server(upstream_name, servers[i].address, &servers[i].params) < 0)
		{
			delete_upstream(upstream_name);
			return -1;
		}
	}

	return 0;
}

uint64_t RpcClient::next_sequence()
{
	static std::atomic<uint64_t> seq(1);
	return seq.fetch_add(1, std::memory_order_relaxed);
}

} // namespace wf_rpc
