#include <stdio.h>
#include <stdlib.h>

#include <vector>

#include "workflow/WFFacilities.h"
#include "workflow/WFGlobal.h"
#include "workflow/Workflow.h"
#include "echo_service_stub.h"

using wf::rpc::example::EchoResponse;
using namespace wf_rpc;

static bool parse_upstream_servers(const char *arg, std::vector<UpstreamServer> *servers)
{
	if (!arg || !servers)
		return false;

	servers->clear();
	std::string spec(arg);
	size_t begin = 0;
	while (begin < spec.size())
	{
		size_t comma = spec.find(',', begin);
		std::string item = spec.substr(begin,
							   comma == std::string::npos ? std::string::npos : comma - begin);
		if (item.empty())
			return false;

		size_t at = item.rfind('@');
		if (at == std::string::npos || at == 0 || at + 1 >= item.size())
			return false;

		std::string address = item.substr(0, at);
		char *end = NULL;
		long weight = strtol(item.c_str() + at + 1, &end, 10);
		if (!end || *end != '\0' || weight <= 0 || weight > 65535)
			return false;

		servers->emplace_back(address, (unsigned short)weight);
		if (comma == std::string::npos)
			break;

		begin = comma + 1;
	}

	return !servers->empty();
}

int main(int argc, char *argv[])
{
	if (argc != 1 && argc != 4 && argc != 5)
	{
		fprintf(stderr,
				"USAGE: %s [host_or_upstream port message] [addr1:port1@w1,addr2:port2@w2,...]\n",
				argv[0]);
		fprintf(stderr,
				"Example with upstream bootstrap:\n"
				"  %s echo.service 9000 hello 127.0.0.1:9000@5,127.0.0.1:9001@1\n",
				argv[0]);
		return 1;
	}

	std::string host = "127.0.0.1";
	unsigned short port = 9000;
	std::string message = "hello";

	if (argc == 4 || argc == 5)
	{
		host = argv[1];
		port = (unsigned short)atoi(argv[2]);
		message = argv[3];
	}

	if (port == 0)
	{
		if (argc == 1)
			fprintf(stderr, "invalid default port\n");
		else
			fprintf(stderr, "invalid port: %s\n", argv[2]);
		return 1;
	}

	if (argc == 5)
	{
		std::vector<UpstreamServer> servers;
		if (!parse_upstream_servers(argv[4], &servers))
		{
			fprintf(stderr,
					"invalid upstream server list, expect addr:port@weight[,addr:port@weight...]\n");
			return 1;
		}

		if (RpcClient::configure_weighted_upstream(host, servers, true) < 0)
		{
			perror("configure_weighted_upstream");
			return 1;
		}
	}

	if (argc == 1)
	{
		printf("rpc_client default call: host=%s port=%u message=%s\n",
			   host.c_str(),
			   port,
			   message.c_str());
	}

	EchoServiceClient client(host, port);
	WFFacilities::WaitGroup wait_group(1);
	RpcTask *task = client.Echo(
		message,
		1,
		[&wait_group](uint32_t status,
					 const EchoResponse& resp,
					 int wf_state,
					 int wf_error,
					 RpcTask *) {
			if (wf_state != WFT_STATE_SUCCESS)
			{
				fprintf(stderr,
						"workflow error: state=%d error=%d (%s)\n",
						wf_state,
						wf_error,
						WFGlobal::get_error_string(wf_state, wf_error));
			}
			else if (status != RPC_OK)
			{
				fprintf(stderr, "rpc status=%u\n", status);
			}
			else
			{
				printf("response: %s\n", resp.message().c_str());
			}
			wait_group.done();
		});

	if (!task)
	{
		fprintf(stderr, "failed to create rpc task\n");
		return 1;
	}

	task->set_send_timeout(3000);
	task->set_receive_timeout(3000);

	Workflow::start_series_work(task, nullptr);
	wait_group.wait();
	return 0;
}
