#include <stdio.h>
#include <stdlib.h>

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
	if (argc != 1 && argc != 4)
	{
		fprintf(stderr, "USAGE: %s [host_or_upstream port message]\n", argv[0]);
		return 1;
	}

	std::string host = "127.0.0.1";
	unsigned short port = 9000;
	std::string message = "hello_easy";

	if (argc == 4)
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

	if (argc == 1)
	{
		printf("rpc_easy_client default call: host=%s port=%u message=%s\n",
			   host.c_str(),
			   port,
			   message.c_str());
	}

	EchoRequest req;
	req.set_message(message);

	EasyRpcClient client(host, port, "wf.rpc.example.EchoService");

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
