#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

#include "echo.pb.h"
#include "tinypb_rpc_controller.h"
#include "tinypb_rpc_async_channel.h"
#include "tinypb_rpc_server.h"

namespace
{

const char* kHost = "127.0.0.1";
const unsigned short kPort = 19001;

const char* kServiceName = "wf.rpc.example.EchoService";
const char* kMethodName = "Echo";
const char* kMessage = "tinypb_qps_test_message";

constexpr int kThreadCases[] = {1, 2, 4, 8, 16, 32};

struct QPSCaseResult
{
    int thread_count;
    int total_requests;
    int success_count;
    int fail_count;
    double seconds;
    double qps;
    double avg_latency_us;
};

class Semaphore
{
public:
    explicit Semaphore(int count = 0) : count_(count) {}

    void wait()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return count_ > 0; });
        --count_;
    }

    void signal()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++count_;
        cv_.notify_one();
    }

private:
    int count_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

std::mutex g_server_mutex;
std::condition_variable g_server_cv;
bool g_server_ready = false;

class EchoServiceImpl : public wf::rpc::example::EchoService
{
public:
    void Echo(google::protobuf::RpcController* controller,
              const wf::rpc::example::EchoRequest* request,
              wf::rpc::example::EchoResponse* response,
              google::protobuf::Closure* done) override
    {
        response->set_message("echo_tinypb: " + request->message());
        if (done)
            done->Run();
    }
};

QPSCaseResult run_qps_case(int thread_count, int requests_per_thread)
{
    std::atomic<int> success_count(0);
    std::atomic<int> fail_count(0);

    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(thread_count));

    auto begin = std::chrono::steady_clock::now();

    for (int i = 0; i < thread_count; ++i)
    {
        workers.emplace_back([requests_per_thread, &success_count, &fail_count]() {
            wf_rpc::TinyPbRpcAsyncChannel channel(kHost, kPort);
            wf::rpc::example::EchoService_Stub stub(&channel);

            for (int n = 0; n < requests_per_thread; ++n)
            {
                wf_rpc::TinyPbRpcController controller;
                controller.SetTimeout(5000);

                wf::rpc::example::EchoRequest request;
                request.set_message(kMessage);

                wf::rpc::example::EchoResponse response;

                Semaphore sem;
                auto done = google::protobuf::NewCallback(&sem, &Semaphore::signal);
                stub.Echo(&controller, &request, &response, done);

                sem.wait();

                if (controller.Failed())
                    fail_count.fetch_add(1, std::memory_order_relaxed);
                else
                    success_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (size_t i = 0; i < workers.size(); ++i)
        workers[i].join();

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end - begin;
    double seconds = elapsed.count();
    int total = thread_count * requests_per_thread;

    QPSCaseResult r;
    r.thread_count = thread_count;
    r.total_requests = total;
    r.success_count = success_count.load();
    r.fail_count = fail_count.load();
    r.seconds = seconds;
    r.qps = seconds > 0.0 ? static_cast<double>(r.success_count) / seconds : 0.0;
    r.avg_latency_us = total > 0 ? seconds * 1000000.0 / static_cast<double>(total) : 0.0;
    return r;
}

void print_case(const QPSCaseResult& r)
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

void wait_for_server_ready()
{
    std::unique_lock<std::mutex> lock(g_server_mutex);
    g_server_cv.wait(lock, [] { return g_server_ready; });
}

void signal_server_ready()
{
    {
        std::lock_guard<std::mutex> lock(g_server_mutex);
        g_server_ready = true;
    }
    g_server_cv.notify_one();
}

}

int main(int argc, char* argv[])
{
    int requests_per_thread = 1000;
    if (argc >= 2)
        requests_per_thread = std::atoi(argv[1]);

    if (requests_per_thread <= 0)
    {
        std::cerr << "invalid requests_per_thread: " << requests_per_thread << "\n";
        return 1;
    }

    google::protobuf::Service* service = new EchoServiceImpl();
    wf_rpc::GetRpcServer()->get_dispatcher()->registerService(service);

    std::thread server_thread([&]() {
        if (wf_rpc::GetRpcServer()->start(kHost, kPort) != 0)
        {
            std::cerr << "server start failed at " << kHost << ":" << kPort << "\n";
            signal_server_ready();
            return;
        }
        signal_server_ready();
    });

    wait_for_server_ready();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "=========================================\n";
    std::cout << " TinyPB RPC QPS Test\n";
    std::cout << "=========================================\n";
    std::cout << "server=" << kHost << ":" << kPort
              << " service=" << kServiceName
              << " method=" << kMethodName
              << " requests_per_thread=" << requests_per_thread
              << "\n\n";

    std::cout << std::setw(8) << "threads"
              << std::setw(10) << "total"
              << std::setw(10) << "success"
              << std::setw(8) << "fail"
              << std::setw(12) << "elapsed"
              << std::setw(12) << "qps"
              << std::setw(16) << "avg_latency"
              << "\n";
    std::cout << "---------------------------------------------------------------\n";

    for (size_t i = 0; i < sizeof(kThreadCases) / sizeof(kThreadCases[0]); ++i)
    {
        QPSCaseResult result = run_qps_case(kThreadCases[i], requests_per_thread);
        print_case(result);
    }

    std::cout << "---------------------------------------------------------------\n";

    wf_rpc::GetRpcServer()->stop();
    server_thread.join();

    return 0;
}
