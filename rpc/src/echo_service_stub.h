#ifndef WF_RPC_ECHO_SERVICE_STUB_H
#define WF_RPC_ECHO_SERVICE_STUB_H

#include <functional>
#include <string>
#include <utility>

#include "rpc_easy.h"
#include "echo.pb.h"

namespace wf_rpc
{

class EchoServiceServer
{
public:
	EchoServiceServer() :
		server_(service_name())
	{
	}

	explicit EchoServiceServer(const struct WFServerParams *params) :
		server_(service_name(), params)
	{
	}

	void register_echo(std::function<void (const wf::rpc::example::EchoRequest&,
						   wf::rpc::example::EchoResponse&)> handler)
	{
		this->server_.register_method<wf::rpc::example::EchoRequest,
						   wf::rpc::example::EchoResponse>(
			echo_method_name(),
			std::move(handler));
	}

	int start(unsigned short port)
	{
		return this->server_.start(port);
	}

	int start(int family, unsigned short port)
	{
		return this->server_.start(family, port);
	}

	int start(const char *host, unsigned short port)
	{
		return this->server_.start(host, port);
	}

	void stop()
	{
		this->server_.stop();
	}

	static const std::string& service_name()
	{
		static const std::string kService = "wf.rpc.example.EchoService";
		return kService;
	}

	static const std::string& echo_method_name()
	{
		static const std::string kEcho = "Echo";
		return kEcho;
	}

private:
	EasyRpcServer server_;
};

class EchoServiceClient
{
public:
	EchoServiceClient(const std::string& host, unsigned short port) :
		client_(host, port, EchoServiceServer::service_name())
	{
	}

	RpcTask *Echo(const std::string& message,
			 int retry_max,
			 std::function<void (uint32_t,
						const wf::rpc::example::EchoResponse&,
						int,
						int,
						RpcTask *)> callback) const
	{
		wf::rpc::example::EchoRequest req;
		req.set_message(message);
		return this->client_.create_task<wf::rpc::example::EchoRequest,
						wf::rpc::example::EchoResponse>(
			EchoServiceServer::echo_method_name(),
			req,
			retry_max,
			std::move(callback));
	}

private:
	EasyRpcClient client_;
};

} // namespace wf_rpc

#endif
