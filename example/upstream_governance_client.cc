#include <stdio.h>
#include <stdlib.h>

#include <vector>

#include "workflow/WFFacilities.h"
#include "workflow/WFGlobal.h"
#include "workflow/Workflow.h"
#include "rpc_easy.h"
#include "echo.pb.h"

using wf::rpc::example::EchoRequest;
using wf::rpc::example::EchoResponse;
using namespace wf_rpc;

int main(int argc, char *argv[])
{
	if (argc != 5)
	{
		fprintf(stderr,
				"USAGE: %s <upstream_name> <port> <message> <address1:port1,address2:port2,...>\n",
				argv[0]);
		return 1;
	}

	std::string upstream = argv[1];
	unsigned short port = (unsigned short)atoi(argv[2]);
	std::string msg = argv[3];
	std::string list = argv[4];

	std::vector<UpstreamServer> servers;
	size_t begin = 0;
	while (begin < list.size())
	{
		size_t comma = list.find(',', begin);
		std::string address = list.substr(begin,
							 comma == std::string::npos ? std::string::npos : comma - begin);
		if (!address.empty())
			servers.emplace_back(address, 1);

		if (comma == std::string::npos)
			break;
		begin = comma + 1;
	}

	if (servers.empty())
	{
		fprintf(stderr, "empty server list\n");
		return 1;
	}

	if (ServiceRegistry::configure_weighted(upstream, servers, true) < 0)
	{
		perror("ServiceRegistry::configure_weighted");
		return 1;
	}

	EasyRpcClient client(upstream, port, "wf.rpc.example.EchoService");
	EchoRequest req;
	req.set_message(msg);

	WFFacilities::WaitGroup wait_group(1);
	RpcTask *task = client.create_task<EchoRequest, EchoResponse>(
		"Echo",
		req,
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
