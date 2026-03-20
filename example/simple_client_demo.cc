#include <iostream>

#include "echo.pb.h"
#include "rpc_easy.h"

namespace
{
// 简单直连示例服务端的固定地址。
const char *kServerHost = "127.0.0.1";
const unsigned short kServerPort = 9000;

// 这两个字符串必须与服务端注册的 service/method 完全一致。
const char *kServiceName = "wf.rpc.example.EchoService";
const char *kMethodName = "Echo";

// 本示例请求使用的业务消息。
const char *kMessage = "hello_simple_rpc";

} // namespace

int main()
{
	// 1) 构造 protobuf 请求对象。
	wf::rpc::example::EchoRequest request;
	request.set_message(kMessage);

	// 2) 使用最小五元组发起远程调用：
	//    host + port + service + method + request。
	//    该封装会阻塞等待回调完成，并通过 SimpleRpcResult 返回
	//    传输层状态与 RPC 业务状态。
	wf::rpc::example::EchoResponse response;
	wf_rpc::SimpleRpcResult result =
		wf_rpc::SimpleRpcClient::call<wf::rpc::example::EchoRequest,
							 wf::rpc::example::EchoResponse>(
			kServerHost,
			kServerPort,
			kServiceName,
			kMethodName,
			request,
			&response,
			1);

	// 3) 同时检查 Workflow 传输层状态和 RPC 业务状态。
	if (result.state == WFT_STATE_SUCCESS && result.status == wf_rpc::RPC_OK)
		std::cout << "simple rpc response: " << response.message() << "\n";
	else
		// 输出完整状态字段，便于排查问题。
		std::cerr << "simple rpc failed: state=" << result.state
			  << " error=" << result.error
			  << " status=" << result.status << "\n";

	return 0;
}
