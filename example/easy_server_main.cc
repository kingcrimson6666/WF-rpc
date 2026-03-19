#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "workflow/WFFacilities.h"
#include "rpc_easy.h"
#include "echo.pb.h"

using wf::rpc::example::EchoRequest;
using wf::rpc::example::EchoResponse;
using namespace wf_rpc;

static WFFacilities::WaitGroup wait_group(1);

static void sig_handler(int)
{
	wait_group.done();
}

int main(int argc, char *argv[])
{
	if (argc != 1 && argc != 2)
	{
		fprintf(stderr, "USAGE: %s [port]\n", argv[0]);
		return 1;
	}

	unsigned short port = 9000;
	if (argc == 2)
		port = (unsigned short)atoi(argv[1]);

	if (port == 0)
	{
		fprintf(stderr, "invalid port\n");
		return 1;
	}

	signal(SIGINT, sig_handler);

	struct WFServerParams params = SERVER_PARAMS_DEFAULT;
	params.request_size_limit = 8 * 1024 * 1024;
	params.receive_timeout = 5000;

	EasyRpcServer server("wf.rpc.example.EchoService", &params);
	server.register_method<EchoRequest, EchoResponse>(
		"Echo",
		[](const EchoRequest& req, EchoResponse& resp) {
			resp.set_message("echo: " + req.message());
		});

	if (server.start(AF_INET6, port) == 0 || server.start(AF_INET, port) == 0)
	{
		printf("easy rpc server started on %u\n", port);
		wait_group.wait();
		server.stop();
		return 0;
	}

	perror("server.start");
	return 1;
}
