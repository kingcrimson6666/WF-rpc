#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>

#include "echo.pb.h"
#include "rpc_easy.h"

namespace
{
// 简单 RPC 服务端绑定地址。
const char *kServerHost = "127.0.0.1";
const unsigned short kServerPort = 9000;

// 对外暴露的 service/method。
const char *kServiceName = "wf.rpc.example.EchoService";
const char *kMethodName = "Echo";

// 信号安全的停止标记，信号处理函数只负责置位。
volatile std::sig_atomic_t g_stop_flag = 0;

void sig_handler(int)
{
	// 处理函数保持最小化，满足异步信号安全要求。
	g_stop_flag = 1;
}

} // namespace

int main()
{
	// 注册 Ctrl+C / 终止信号，实现优雅退出。
	std::signal(SIGINT, sig_handler);
	std::signal(SIGTERM, sig_handler);

	// 创建一个绑定固定 host/port/service 的服务实例。
	wf_rpc::SimpleRpcServer server(kServerHost, kServerPort, kServiceName);

	// 注册 protobuf 方法处理器。
	// lambda 内完成 request 到 response 的转换。
	server.register_method<wf::rpc::example::EchoRequest, wf::rpc::example::EchoResponse>(
		kMethodName,
		[](const wf::rpc::example::EchoRequest& req,
		   wf::rpc::example::EchoResponse& resp) {
			resp.set_message("echo_simple: " + req.message());
		});

	// 启动监听。
	if (server.start() != 0)
	{
		std::cerr << "failed to start simple server at " << kServerHost
			  << ":" << kServerPort << "\n";
		return 1;
	}

	std::cout << "simple server started at " << kServerHost << ":" << kServerPort
		  << " service=" << kServiceName << " method=" << kMethodName << "\n";
	std::cout << "press Ctrl+C to stop\n";

	// 在普通线程里轮询信号标记，并桥接到
	// SimpleRpcServer 的生命周期接口。
	std::thread watcher([&server]() {
		while (!g_stop_flag)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		server.request_stop();
	});

	// 阻塞等待停止请求，然后执行有序关闭。
	server.wait_for_stop();
	server.stop();
	watcher.join();
	return 0;
}
