#include <iostream>
#include <vector>

#include "echo.pb.h"
#include "rpc_easy.h"

namespace
{
// Workflow 路由策略使用的本地 upstream 名称。
const char *kUpstreamName = "wf-rpc-upstream-demo";

// 创建客户端目标键时使用的端口。
// 与示例 upstream 服务端端口保持一致。
const unsigned short kUpstreamPort = 9100;

// 服务端示例注册的 RPC 路由标识。
const char *kServiceName = "wf.rpc.example.EchoService";
const char *kMethodName = "Echo";

// 示例请求消息。
const char *kMessage = "hello_upstream_rpc";

// 该 upstream 组的加权后端列表。
// 9100 被选中的概率高于 9101。
const std::vector<wf_rpc::UpstreamServer> kServers = {
	{"127.0.0.1:9100", 5},
	{"127.0.0.1:9101", 1},
};

} // namespace

int main()
{
	// 1) 在当前进程内创建本地加权 upstream 策略。
	if (wf_rpc::ServiceRegistry::configure_weighted(kUpstreamName, kServers, true) != 0)
	{
		std::cerr << "failed to configure upstream: " << kUpstreamName << "\n";
		return 1;
	}

	// 2) 构造 protobuf 请求对象。
	wf::rpc::example::EchoRequest request;
	request.set_message(kMessage);

	// 3) 通过 upstream 名称 + 端口发起 RPC。
	// 底层路由策略会自动选出一个后端服务。
	wf::rpc::example::EchoResponse response;
	wf_rpc::SimpleRpcResult result =
		wf_rpc::SimpleRpcClient::call<wf::rpc::example::EchoRequest,
						 wf::rpc::example::EchoResponse>(
			kUpstreamName,
			kUpstreamPort,
			kServiceName,
			kMethodName,
			request,
			&response,
			1);

	// 4) 检查传输层与 RPC 层状态。
	if (result.state == WFT_STATE_SUCCESS && result.status == wf_rpc::RPC_OK)
		std::cout << "upstream rpc response: " << response.message() << "\n";
	else
		// 输出完整状态字段，便于快速定位故障。
		std::cerr << "upstream rpc failed: state=" << result.state
			  << " error=" << result.error
			  << " status=" << result.status << "\n";

	// 5) 清理临时 upstream 配置，避免污染进程内状态。
	wf_rpc::ServiceRegistry::remove_service(kUpstreamName);
	return 0;
}
