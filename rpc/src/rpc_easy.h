#ifndef WF_RPC_EASY_H
#define WF_RPC_EASY_H

#include <functional>
#include <string>
#include <cstdint>
#include <condition_variable>
#include <mutex>
#include <utility>
#include <vector>

#include "rpc_framework.h"
#include "workflow/WFFacilities.h"

namespace wf_rpc
{

class EasyRpcServer
{
public:
	explicit EasyRpcServer(const std::string& service_name);
	EasyRpcServer(const std::string& service_name,
			  const struct WFServerParams *params);

	template<class Req, class Resp>
	void register_method(const std::string& method,
				 std::function<void (const Req&, Resp&)> handler)
	{
		this->server_.register_pb_method<Req, Resp>(this->service_name_,
								   method,
								   std::move(handler));
	}

	int start(unsigned short port);
	int start(int family, unsigned short port);
	int start(const char *host, unsigned short port);
	void stop();

	const std::string& service_name() const
	{
		return this->service_name_;
	}

private:
	std::string service_name_;
	RpcServer server_;
};

class EasyRpcClient
{
public:
	EasyRpcClient(const std::string& host,
			  unsigned short port,
			  const std::string& service_name);

	template<class Req, class Resp>
	RpcTask *create_task(const std::string& method,
				 const Req& request,
				 int retry_max,
				 std::function<void (uint32_t,
							 const Resp&,
							 int,
							 int,
							 RpcTask *)> callback) const
	{
		return RpcClient::create_pb_task<Req, Resp>(this->host_,
								  this->port_,
								  this->service_name_,
								  method,
								  request,
								  retry_max,
								  std::move(callback));
	}

	template<class Req, class Resp>
	RpcTask *create_task_by_url(const std::string& url,
					const std::string& method,
					const Req& request,
					int retry_max,
					std::function<void (uint32_t,
							   const Resp&,
							   int,
							   int,
							   RpcTask *)> callback) const
	{
		return RpcClient::create_pb_task_by_url<Req, Resp>(url,
								 this->service_name_,
								 method,
								 request,
								 retry_max,
								 std::move(callback));
	}

private:
	std::string host_;
	unsigned short port_;
	std::string service_name_;
};

struct SimpleRpcResult
{
	uint32_t status;
	int state;
	int error;

	SimpleRpcResult() :
		status(RPC_NETWORK_ERROR),
		state(-1),
		error(0)
	{
	}
};

class SimpleRpcServer
{
public:
	SimpleRpcServer(const std::string& host,
			 unsigned short port,
			 const std::string& service_name) :
		host_(host),
		port_(port),
		server_(service_name),
		stop_requested_(false)
	{
	}

	template<class Req, class Resp>
	void register_method(const std::string& method,
				 std::function<void (const Req&, Resp&)> handler)
	{
		this->server_.register_method<Req, Resp>(method, std::move(handler));
	}

	int start()
	{
		std::lock_guard<std::mutex> lock(this->lifecycle_mutex_);
		this->stop_requested_ = false;
		return this->server_.start(this->host_.c_str(), this->port_);
	}

	int run_until_stopped();

	void request_stop();

	void wait_for_stop();

	void stop()
	{
		this->server_.stop();
	}

private:
	std::string host_;
	unsigned short port_;
	EasyRpcServer server_;
	std::mutex lifecycle_mutex_;
	std::condition_variable lifecycle_cv_;
	bool stop_requested_;
};

class SimpleRpcClient
{
public:
	template<class Req, class Resp>
	static SimpleRpcResult call(const std::string& host,
					unsigned short port,
					const std::string& service,
					const std::string& method,
					const Req& request,
					Resp *response,
					int retry_max = 1)
	{
		SimpleRpcResult result;
		WFFacilities::WaitGroup wait_group(1);

		RpcTask *task = RpcClient::create_pb_task<Req, Resp>(
			host,
			port,
			service,
			method,
			request,
			retry_max,
			[&result, response, &wait_group](uint32_t status,
								 const Resp& resp,
								 int state,
								 int error,
								 RpcTask *) {
				result.status = status;
				result.state = state;
				result.error = error;
				if (response && state == WFT_STATE_SUCCESS && status == RPC_OK)
					*response = resp;
				wait_group.done();
			});

		if (!task)
		{
			result.error = -1;
			return result;
		}

		task->start();
		wait_group.wait();
		return result;
	}

	template<class Req, class Resp>
	static SimpleRpcResult call_by_url(const std::string& url,
					   const std::string& service,
					   const std::string& method,
					   const Req& request,
					   Resp *response,
					   int retry_max = 1)
	{
		SimpleRpcResult result;
		WFFacilities::WaitGroup wait_group(1);

		RpcTask *task = RpcClient::create_pb_task_by_url<Req, Resp>(
			url,
			service,
			method,
			request,
			retry_max,
			[&result, response, &wait_group](uint32_t status,
								 const Resp& resp,
								 int state,
								 int error,
								 RpcTask *) {
				result.status = status;
				result.state = state;
				result.error = error;
				if (response && state == WFT_STATE_SUCCESS && status == RPC_OK)
					*response = resp;
				wait_group.done();
			});

		if (!task)
		{
			result.error = -1;
			return result;
		}

		task->start();
		wait_group.wait();
		return result;
	}
};

class ServiceRegistry
{
public:
	static int create_weighted(const std::string& upstream_name,
				   bool try_another);

	static int register_server(const std::string& upstream_name,
				 const std::string& address);

	static int register_server(const std::string& upstream_name,
				 const std::string& address,
				 const struct AddressParams *params);

	static int unregister_server(const std::string& upstream_name,
				   const std::string& address);

	static int remove_service(const std::string& upstream_name);

	static int configure_weighted(const std::string& upstream_name,
				    const std::vector<UpstreamServer>& servers,
				    bool try_another);

	static int configure_weighted(const std::string& upstream_name,
				    const std::vector<UpstreamServerParams>& servers,
				    bool try_another);
};

} // namespace wf_rpc

#endif
