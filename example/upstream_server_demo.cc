#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>

#include "echo.pb.h"
#include "rpc_easy.h"

namespace
{
// 两个示例后端共用的绑定地址。
const char *kServerHost = "127.0.0.1";

// 用于 upstream 负载均衡演示的两个后端实例。
const unsigned short kServerPortA = 9100;
const unsigned short kServerPortB = 9101;

// 两个后端暴露同一组 RPC 路由。
const char *kServiceName = "wf.rpc.example.EchoService";
const char *kMethodName = "Echo";

// 信号安全停止标记，由信号处理函数置位。
volatile std::sig_atomic_t g_stop_flag = 0;

void sig_handler(int)
{
	// 信号处理逻辑保持最小且安全。
	g_stop_flag = 1;
}

} // namespace

int main()
{
	// 安装信号处理函数，实现进程优雅退出。
	std::signal(SIGINT, sig_handler);
	std::signal(SIGTERM, sig_handler);

	// 后端 A（端口 9100）。
	wf_rpc::SimpleRpcServer server_a(kServerHost, kServerPortA, kServiceName);
	server_a.register_method<wf::rpc::example::EchoRequest, wf::rpc::example::EchoResponse>(
		kMethodName,
		[](const wf::rpc::example::EchoRequest& req,
		   wf::rpc::example::EchoResponse& resp) {
			// 响应前缀用于在客户端侧识别命中的后端。
			resp.set_message("from_9100: " + req.message());
		});

	// 后端 B（端口 9101）。
	wf_rpc::SimpleRpcServer server_b(kServerHost, kServerPortB, kServiceName);
	server_b.register_method<wf::rpc::example::EchoRequest, wf::rpc::example::EchoResponse>(
		kMethodName,
		[](const wf::rpc::example::EchoRequest& req,
		   wf::rpc::example::EchoResponse& resp) {
			// 响应前缀用于在客户端侧识别命中的后端。
			resp.set_message("from_9101: " + req.message());
		});

	// 先启动后端 A。
	if (server_a.start() != 0)
	{
		std::cerr << "failed to start upstream server 9100\n";
		return 1;
	}

	// 再启动后端 B；若失败则回滚关闭 A，保持状态一致。
	if (server_b.start() != 0)
	{
		std::cerr << "failed to start upstream server 9101\n";
		server_a.stop();
		return 1;
	}

	std::cout << "upstream servers started: "
		  << kServerHost << ":" << kServerPortA << ", "
		  << kServerHost << ":" << kServerPortB << "\n";
	std::cout << "service=" << kServiceName << " method=" << kMethodName << "\n";
	std::cout << "press Ctrl+C to stop\n";

	// 在普通线程中轮询停止标记，并通知两个服务实例。
	std::thread watcher([&server_a, &server_b]() {
		while (!g_stop_flag)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		server_a.request_stop();
		server_b.request_stop();
	});

	// 按顺序等待并关闭两个服务实例。
	server_a.wait_for_stop();
	server_a.stop();
	server_b.wait_for_stop();
	server_b.stop();
	watcher.join();
	return 0;
}
