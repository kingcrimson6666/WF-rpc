#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include "echo.pb.h"
#include "rpc_easy.h"

namespace
{
// 测试固定在本机回环地址，避免外部网络抖动影响结果。
const char *kHost = "127.0.0.1";
const unsigned short kPort = 19000;

// 与 proto 中定义保持一致的 service/method 名称。
const char *kServiceName = "wf.rpc.example.EchoService";
const char *kMethodName = "Echo";
// 每次请求发送的业务消息内容，本测试只关注吞吐，不关注复杂业务逻辑。
const char *kMessage = "qps_test_message";

// 单个线程档位（例如 1/2/4/8/16/32）下的一次完整测试结果。
struct CaseResult
{
	// 本档位并发线程数。
	int thread_count;
	// 本档位总请求数 = thread_count * requests_per_thread。
	int total_requests;
	// 成功请求数量（state + rpc status 都成功才计入）。
	int success_count;
	// 失败请求数量（网络失败、超时、RPC 非 OK 等都计入）。
	int fail_count;
	// 整个档位从发起到全部完成的总耗时（秒）。
	double seconds;
	// 每秒成功请求数：success_count / seconds。
	double qps;
	// 平均时延（微秒）：seconds * 1e6 / total_requests。
	double avg_latency_us;
};

// 运行一个线程档位的压测。
// 线程内是串行同步调用，线程间并发执行。
CaseResult run_qps_case(int thread_count, int requests_per_thread)
{
	// 原子计数器用于跨线程安全统计成功/失败数量。
	std::atomic<int> success_count(0);
	std::atomic<int> fail_count(0);

	// 保存 worker 线程句柄，后续统一 join。
	std::vector<std::thread> workers;
	workers.reserve(static_cast<size_t>(thread_count));

	// 计时起点：在创建并发线程之前记录。
	auto begin = std::chrono::steady_clock::now();
	for (int i = 0; i < thread_count; ++i)
	{
		// 每个线程各自完成 requests_per_thread 次同步 RPC 调用。
		workers.emplace_back([requests_per_thread, &success_count, &fail_count]() {
			for (int n = 0; n < requests_per_thread; ++n)
			{
				// 每次循环构造新的请求/响应对象，避免共享状态。
				wf::rpc::example::EchoRequest request;
				request.set_message(kMessage);
				wf::rpc::example::EchoResponse response;

				// 通过简化封装发起一次 RPC，同步等待返回。
				wf_rpc::SimpleRpcResult result =
					wf_rpc::SimpleRpcClient::call<wf::rpc::example::EchoRequest,
								     wf::rpc::example::EchoResponse>(
						kHost,
						kPort,
						kServiceName,
						kMethodName,
						request,
						&response,
						1);

				// 仅当传输层成功且 RPC 业务状态 OK 时计为成功。
				if (result.state == WFT_STATE_SUCCESS && result.status == wf_rpc::RPC_OK)
					success_count.fetch_add(1, std::memory_order_relaxed);
				else
					fail_count.fetch_add(1, std::memory_order_relaxed);
			}
		});
	}

	for (size_t i = 0; i < workers.size(); ++i)
		workers[i].join();

	// 计时终点：全部线程完成后记录。
	auto end = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed = end - begin;
	double seconds = elapsed.count();
	int total = thread_count * requests_per_thread;

	// 汇总并计算衍生指标。
	CaseResult r;
	r.thread_count = thread_count;
	r.total_requests = total;
	r.success_count = success_count.load();
	r.fail_count = fail_count.load();
	r.seconds = seconds;
	// QPS 按“成功请求数”计算，避免失败请求虚高吞吐。
	r.qps = seconds > 0.0 ? static_cast<double>(r.success_count) / seconds : 0.0;
	// 平均时延按“总请求数”均摊，反映整体请求成本。
	r.avg_latency_us = total > 0 ? seconds * 1000000.0 / static_cast<double>(total) : 0.0;
	return r;
}

// 统一打印一个档位的结果，便于横向比较不同线程数表现。
void print_case(const CaseResult& r)
{
	std::cout << std::fixed << std::setprecision(2)
		  << "threads=" << std::setw(2) << r.thread_count
		  << " total=" << std::setw(7) << r.total_requests
		  << " success=" << std::setw(7) << r.success_count
		  << " fail=" << std::setw(5) << r.fail_count
		  << " elapsed=" << std::setw(8) << r.seconds << "s"
		  << " qps=" << std::setw(10) << r.qps
		  << " avg_latency=" << std::setw(10) << r.avg_latency_us << "us"
		  << "\n";
}

} // namespace

int main(int argc, char *argv[])
{
	// 默认每线程发 1000 请求，可通过命令行参数覆盖：
	// ./rpc_single_client_server_qps_test 5000
	int requests_per_thread = 1000;
	if (argc >= 2)
		requests_per_thread = std::atoi(argv[1]);

	// 防御式校验，防止无效参数造成异常结果。
	if (requests_per_thread <= 0)
	{
		std::cerr << "invalid requests_per_thread: " << requests_per_thread << "\n";
		return 1;
	}

	// 启动单个本地 RPC 服务端（single server）。
	wf_rpc::SimpleRpcServer server(kHost, kPort, kServiceName);
	// 注册一个最轻量 Echo 方法，把请求消息直接回写到响应，
	// 以减少业务逻辑对吞吐测试的干扰。
	server.register_method<wf::rpc::example::EchoRequest, wf::rpc::example::EchoResponse>(
		kMethodName,
		[](const wf::rpc::example::EchoRequest& req,
		   wf::rpc::example::EchoResponse& resp) {
			resp.set_message(req.message());
		});

	// 监听失败直接退出，避免得到无意义压测结果。
	if (server.start() != 0)
	{
		std::cerr << "server start failed at " << kHost << ":" << kPort << "\n";
		return 1;
	}

	// 给服务端一个短暂就绪时间，降低刚启动阶段的抖动。
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	std::cout << "single client + single server local QPS test\n";
	std::cout << "server=" << kHost << ":" << kPort
		  << " service=" << kServiceName
		  << " method=" << kMethodName
		  << " requests_per_thread=" << requests_per_thread
		  << "\n";

	// 固定线程档位，观察并发提升时吞吐和时延变化趋势。
	const std::vector<int> thread_cases = {1, 2, 4, 8, 16, 32};
	for (size_t i = 0; i < thread_cases.size(); ++i)
	{
		// 单 client（当前程序）在该线程档位下发压并输出统计结果。
		CaseResult result = run_qps_case(thread_cases[i], requests_per_thread);
		print_case(result);
	}

	// 测试结束后主动停止服务端，释放端口和相关资源。
	server.stop();
	return 0;
}
