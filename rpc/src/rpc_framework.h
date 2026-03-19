#ifndef WF_RPC_FRAMEWORK_H
#define WF_RPC_FRAMEWORK_H

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "google/protobuf/message_lite.h"
#include "workflow/WFServer.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/UpstreamManager.h"
#include "rpc_message.h"

namespace wf_rpc
{

enum RpcStatus
{
	RPC_OK = 0,
	RPC_BAD_REQUEST = 400,
	RPC_NOT_FOUND = 404,
	RPC_PROTO_PARSE_ERROR = 422,
	RPC_PROTO_SERIALIZE_ERROR = 423,
	RPC_INTERNAL_ERROR = 500,
	RPC_NETWORK_ERROR = 503
};

using RpcTask = WFNetworkTask<RpcRequest, RpcResponse>;
using rpc_callback_t = std::function<void (RpcTask *)>;
using binary_handler_t = std::function<uint32_t (const std::string&, std::string *)>;

class RpcServer
{
public:
	RpcServer();
	explicit RpcServer(const struct WFServerParams *params);
	~RpcServer();

	int start(unsigned short port);
	int start(int family, unsigned short port);
	int start(const char *host, unsigned short port);
	void stop();

	void register_binary_method(const std::string& service,
						   const std::string& method,
						   binary_handler_t handler);

	template<class Req, class Resp>
	void register_pb_method(const std::string& service,
							const std::string& method,
							std::function<void (const Req&, Resp&)> handler)
	{
		this->register_binary_method(service, method,
			[handler](const std::string& request_bin, std::string *response_bin) {
				Req req;
				Resp resp;
				if (!req.ParseFromString(request_bin))
					return (uint32_t)RPC_PROTO_PARSE_ERROR;

				handler(req, resp);
				if (!resp.SerializeToString(response_bin))
					return (uint32_t)RPC_PROTO_SERIALIZE_ERROR;

				return (uint32_t)RPC_OK;
			});
	}

private:
	using RpcWfServer = WFServer<RpcRequest, RpcResponse>;

	void on_process(RpcTask *task);
	static std::string key_of(const std::string& service,
						  const std::string& method);

private:
	std::unordered_map<std::string, binary_handler_t> handlers_;
	std::unique_ptr<RpcWfServer> server_;
};

struct UpstreamServer
{
	std::string address;
	unsigned short weight;

	UpstreamServer(const std::string& addr, unsigned short w) :
		address(addr),
		weight(w)
	{
	}
};

struct UpstreamServerParams
{
	std::string address;
	struct AddressParams params;

	UpstreamServerParams(const std::string& addr,
				 const struct AddressParams& p) :
		address(addr),
		params(p)
	{
	}
};

class RpcClient
{
public:
	static RpcTask *create_task(const std::string& host,
						unsigned short port,
						int retry_max,
						rpc_callback_t callback);

	static RpcTask *create_task_by_url(const std::string& url,
							   int retry_max,
							   rpc_callback_t callback);

	template<class Req, class Resp>
	static RpcTask *create_pb_task_by_url(const std::string& url,
							const std::string& service,
							const std::string& method,
							const Req& request,
							int retry_max,
							std::function<void (uint32_t,
												const Resp&,
												int,
												int,
												RpcTask *)> callback)
	{
		std::string body;
		if (!request.SerializeToString(&body))
			return NULL;

		auto wf_cb = [callback](RpcTask *task) {
			Resp resp;
			uint32_t status = RPC_NETWORK_ERROR;
			if (task->get_state() == WFT_STATE_SUCCESS)
			{
				status = task->get_resp()->get_status();
				if (status == RPC_OK &&
					!resp.ParseFromString(task->get_resp()->payload()))
				{
					status = RPC_PROTO_PARSE_ERROR;
				}
			}

			callback(status,
					 resp,
					 task->get_state(),
					 task->get_error(),
					 task);
		};

		RpcTask *task = create_task_by_url(url, retry_max, std::move(wf_cb));
		if (!task)
			return NULL;

		task->get_req()->set_sequence(next_sequence());
		if (task->get_req()->set_service_method(service, method) < 0)
		{
			task->dismiss();
			return NULL;
		}

		task->get_req()->set_payload(std::move(body));
		return task;
	}

	static int create_weighted_upstream(const std::string& upstream_name,
							 bool try_another);
	static int create_consistent_hash_upstream(const std::string& upstream_name,
								upstream_route_t consitent_hash);
	static int create_manual_upstream(const std::string& upstream_name,
						 upstream_route_t select,
						 bool try_another,
						 upstream_route_t consitent_hash);
	static int create_vnswrr_upstream(const std::string& upstream_name);
	static int delete_upstream(const std::string& upstream_name);

	static int add_upstream_server(const std::string& upstream_name,
						   const std::string& address);
	static int add_upstream_server(const std::string& upstream_name,
						   const std::string& address,
						   const struct AddressParams *params);
	static int remove_upstream_server(const std::string& upstream_name,
							  const std::string& address);

	template<class Req, class Resp>
	static RpcTask *create_pb_task(const std::string& host,
							unsigned short port,
							const std::string& service,
							const std::string& method,
							const Req& request,
							int retry_max,
							std::function<void (uint32_t,
												const Resp&,
												int,
												int,
												RpcTask *)> callback)
	{
		std::string body;
		if (!request.SerializeToString(&body))
			return NULL;

		auto wf_cb = [callback](RpcTask *task) {
			Resp resp;
			uint32_t status = RPC_NETWORK_ERROR;
			if (task->get_state() == WFT_STATE_SUCCESS)
			{
				status = task->get_resp()->get_status();
				if (status == RPC_OK &&
					!resp.ParseFromString(task->get_resp()->payload()))
				{
					status = RPC_PROTO_PARSE_ERROR;
				}
			}

			callback(status,
					 resp,
					 task->get_state(),
					 task->get_error(),
					 task);
		};

		RpcTask *task = create_task(host, port, retry_max, std::move(wf_cb));
		if (!task)
			return NULL;

		task->get_req()->set_sequence(next_sequence());
		if (task->get_req()->set_service_method(service, method) < 0)
		{
			task->dismiss();
			return NULL;
		}

		task->get_req()->set_payload(std::move(body));
		return task;
	}

	static int configure_weighted_upstream(const std::string& upstream_name,
									   const std::vector<UpstreamServer>& servers,
									   bool try_another);

	static int configure_weighted_upstream(const std::string& upstream_name,
									   const std::vector<UpstreamServerParams>& servers,
									   bool try_another);

private:
	static uint64_t next_sequence();
};

} // namespace wf_rpc

#endif
