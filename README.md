# workflow-rpc

基于 Workflow 与 Protobuf 的高性能 RPC 框架，包含两种 RPC 模式：

- **Simple RPC**：轻量级直连调用
- **TinyPB RPC**：基于 protobuf 的完整 RPC 实现，支持服务注册、异步调用、负载均衡

项目目标是帮助你快速完成从"定义 proto -> 启动服务 -> 发起调用 -> 做吞吐测试"的完整闭环。

## 1. 项目结构

```
project/
├── CMakeLists.txt                 # 根 CMake 构建入口（推荐）
├── Makefile                       # 兼容 make 构建入口
├── proto/
│   └── echo.proto                 # 示例 RPC 协议定义
├── rpc/
│   ├── include/
│   │   └── wf_rpc/
│   │       └── tinypb.h          # TinyPB 公共接口
│   └── src/
│       ├── tinypb_codec.*       # TinyPB 协议编解码器
│       ├── tinypb_struct.*      # TinyPB 数据结构
│       ├── tinypb_constants.*   # TinyPB 常量定义
│       ├── tinypb_rpc_server.*  # TinyPB 服务端
│       ├── tinypb_rpc_channel.* # TinyPB 同步客户端通道
│       ├── tinypb_rpc_async_channel.*  # TinyPB 异步客户端通道
│       ├── tinypb_rpc_controller.*    # RPC 控制器
│       ├── tinypb_rpc_dispatcher.*     # 服务分发器
│       ├── tinypb_rpc_upstream.*       # Upstream 管理
│       ├── rpc_message.*        # RPC 消息编解码
│       ├── rpc_framework.*      # 底层 RPC server/client/upstream 封装
│       ├── rpc_easy.*           # 便捷 API（SimpleRpcServer/Client）
│       └── rpc_config.*         # XML 配置加载
├── example/
│   ├── simple_server_demo.cc        # Simple RPC 服务端示例
│   ├── simple_client_demo.cc         # Simple RPC 客户端示例
│   ├── upstream_server_demo.cc        # Upstream 双后端示例
│   ├── upstream_client_demo.cc       # Upstream 负载均衡客户端
│   ├── tinypb_server_demo.cc         # TinyPB 服务端示例
│   ├── tinypb_client_demo.cc         # TinyPB 同步客户端示例
│   ├── tinypb_async_client_demo.cc   # TinyPB 异步客户端示例
│   └── tinypb_loadbalance_demo.cc    # TinyPB 负载均衡综合示例
├── test/
│   ├── single_client_server_qps_test.cc  # 多线程 QPS 压测
│   └── tinypb_unittest.cc               # TinyPB 单元测试
├── scripts/
│   └── build_proto_examples.sh    # make 方式下构建 proto + example 脚本
├── conf/
│   └── (配置文件目录)
└── workflow/                      # Workflow 异步框架子模块
```

## 2. 核心能力

### 2.1 两种 RPC 模式对比

| 特性 | Simple RPC | TinyPB RPC |
|------|------------|------------|
| **协议格式** | 自定义头部 + protobuf | TinyPB 二进制协议 |
| **服务定义** | 运行时注册 | protoc 生成服务基类 |
| **客户端调用** | `call(host, port, service, method, req, resp)` | `stub->method(controller, req, resp, done)` |
| **服务端注册** | `register_method<Req, Resp>("method", handler)` | `REGISTER_SERVICE(ServiceImpl)` |
| **异步支持** | 不支持 | 支持异步通道 |
| **适用场景** | 快速原型、简单调用 | 生产级服务 |

### 2.2 TinyPB 协议格式

```
| start(1B=0x02) | pk_len(4B) | msg_req_len(4B) | msg_req(NB) |
| service_name_len(4B) | service_full_name(NB) | err_code(4B) |
| err_info_len(4B) | err_info(NB) | pb_data(NB) | checksum(4B) | end(1B=0x03) |
```

### 2.3 RPC 状态语义

`rpc/src/rpc_framework.h` 定义了统一状态码：

- `RPC_OK`：成功
- `RPC_BAD_REQUEST`：非法请求
- `RPC_NOT_FOUND`：找不到 service/method
- `RPC_PROTO_PARSE_ERROR`：请求/响应 protobuf 解析失败
- `RPC_PROTO_SERIALIZE_ERROR`：响应序列化失败
- `RPC_INTERNAL_ERROR`：服务端内部错误
- `RPC_NETWORK_ERROR`：网络层错误

客户端返回还包含 Workflow 运行态：

- `state == WFT_STATE_SUCCESS` 表示传输层成功
- `status == RPC_OK` 表示 RPC 业务成功

通常需要两者都成功才算一次完整成功调用。

### 2.4 服务治理能力

| 能力 | API | 说明 |
|------|-----|------|
| **加权随机** | `create_weighted_upstream()` | 按权重随机选择后端 |
| **一致性哈希** | `create_consistent_hash_upstream()` | 相同 key 路由到相同后端 |
| **VNSWRR** | `create_vnswrr_upstream()` | 平滑加权轮询 |
| **动态注册** | `add_upstream_server()` | 运行时添加后端 |
| **动态注销** | `remove_upstream_server()` | 运行时移除后端 |
| **故障转移** | `try_another` 参数 | 失败时尝试其他后端 |

### 2.5 便捷 API

`rpc/src/rpc_easy.h` 提供了三组高层封装：

- `SimpleRpcServer`：快速注册 protobuf 方法并启动服务
- `SimpleRpcClient`：同步阻塞调用，返回 `SimpleRpcResult`
- `ServiceRegistry`：配置/清理 upstream 路由（加权后端等）

## 3. 核心组件详解

### 3.1 TinyPB 服务端组件

```
TinyPbRpcServer
├── ServerType (WFServer<protocol::TLVMessage, protocol::TLVMessage>)
├── TinyPbRpcDispatcher
│   ├── service_map_ (服务名 -> Service 映射)
│   └── mutex_ (线程安全保护)
└── registerService() / dispatch()
```

### 3.2 TinyPB 客户端组件

```
TinyPbRpcChannel (同步通道)
├── host_/port_ 或 url_ (连接信息)
├── use_upstream_ (是否使用 upstream)
└── CallMethod() -> 网络任务 -> 阻塞等待响应

TinyPbRpcAsyncChannel (异步通道)
├── resp_msg (unique_ptr 管理响应消息)
└── CallMethod() -> 异步回调
```

### 3.3 TinyPbRpcController

控制 RPC 调用的上下文：

- `SetTimeout() / GetTimeout()`：超时控制
- `SetFailed() / Failed()`：错误状态
- `SetErrorCode() / GetErrorCode()`：错误码
- `SetMsgReq() / GetMsgReq()`：消息序列号

### 3.4 TinyPbRpcDispatcher

服务请求分发器：

- `registerService()`：注册 protobuf 服务
- `dispatch()`：根据 service_full_name 路由到对应服务方法

## 4. 环境依赖

建议在 Linux 下准备：

- C++ 编译器：`g++`（支持 C++11）
- CMake：`>= 3.16`
- GNU Make
- Protobuf：`protoc` + C++ 库
- OpenSSL 开发库
- pthread / dl / rt（Linux 常见系统库）

如果你使用 Ubuntu，可参考：

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake make \
  protobuf-compiler libprotobuf-dev \
  libssl-dev
```

## 5. 构建方式

### 5.1 方式 A：CMake（推荐）

```bash
cd project
rm -rf build
mkdir build && cd build
cmake ..
make -j$(nproc)
```

构建完成后，主要可执行文件在 `build/` 下：

| 可执行文件 | 说明 |
|-----------|------|
| `rpc_simple_server_demo` | Simple RPC 服务端 |
| `rpc_simple_client_demo` | Simple RPC 客户端 |
| `rpc_upstream_server_demo` | Upstream 双后端服务端 |
| `rpc_upstream_client_demo` | Upstream 负载均衡客户端 |
| `rpc_tinypb_server_demo` | TinyPB RPC 服务端 |
| `rpc_tinypb_client_demo` | TinyPB RPC 同步客户端 |
| `rpc_tinypb_async_client_demo` | TinyPB RPC 异步客户端 |
| `rpc_single_client_server_qps_test` | 多线程 QPS 压测（Simple RPC） |
| `rpc_tinypb_qps_test` | 多线程 QPS 压测（ TinyPB RPC） |
| `rpc_tinypb_loadbalance_demo` | TinyPB 负载均衡综合示例 |

### 5.2 方式 B：Make + 脚本

```bash
cd project
make                # 构建 workflow 与 rpc core
make proto_examples # 调用 scripts/build_proto_examples.sh 构建 proto + example
```

说明：

- `make` 主要产出核心静态库（如 `build/libworkflow_rpc_core.a`）
- `make proto_examples` 会生成并编译 `echo.pb.cc/.h` 及示例二进制

### 5.3 方式 C：Docker / Docker Compose（容器化推荐）

仓库已提供以下容器文件：

- `Dockerfile`：多阶段构建（builder + runtime）
- `docker-compose.yml`：按场景编排 `simple` / `upstream` / `qps`
- `.dockerignore`：缩小构建上下文

#### 5.3.1 构建镜像

```bash
cd project
docker build -t workflow-rpc-demo:latest .
```

#### 5.3.2 运行 simple 场景

```bash
cd project
docker compose --profile simple up --build --abort-on-container-exit simple-server simple-client
```

说明：

- `simple-client` 运行后会退出；可通过日志看到响应输出
- 如需清理容器：

```bash
docker compose --profile simple down
```

#### 5.3.3 运行 upstream 场景

```bash
cd project
docker compose --profile upstream up --build --abort-on-container-exit upstream-server upstream-client
```

清理：

```bash
docker compose --profile upstream down
```

#### 5.3.4 运行 QPS 场景

默认每线程 1000 请求：

```bash
cd project
docker compose --profile qps run --build --rm qps-test
```

自定义每线程请求数（例如 5000）：

```bash
cd project
docker compose --profile qps run --build --rm qps-test ./rpc_single_client_server_qps_test 5000
```

### 5.3.5 容器化参数说明

`Dockerfile` 在构建阶段默认关闭了 Workflow 的可选模块：

- `KAFKA=n`
- `MYSQL=n`
- `REDIS=n`
- `CONSUL=n`

并保留 `UPSTREAM=y`，以支持 upstream 示例。

注意：当前示例程序中的 client/server 目标地址使用回环地址（`127.0.0.1`），Compose 已通过共享网络命名空间确保示例可直接运行。

## 6. 快速运行

### 6.1 Simple RPC 示例

终端 1（服务端）：

```bash
cd project/build
./rpc_simple_server_demo
```

终端 2（客户端）：

```bash
cd project/build
./rpc_simple_client_demo
```

预期：客户端输出类似 `simple rpc response: echo_simple: hello_simple_rpc`。

### 6.2 TinyPB RPC 同步示例

终端 1（服务端）：

```bash
cd project/build
./rpc_tinypb_server_demo
```

终端 2（同步客户端）：

```bash
cd project/build
./rpc_tinypb_client_demo
```

预期：客户端输出 `Response: echo_tinypb: hello_tinypb`

### 6.3 TinyPB RPC 异步示例

```bash
cd project/build
./rpc_tinypb_async_client_demo
```

预期：客户端输出 `Async Response: echo_tinypb: hello_async_tinypb`

### 6.4 upstream 加权路由示例

终端 1（启动两个后端）：

```bash
cd project/build
./rpc_upstream_server_demo
```

终端 2（通过 upstream 调用）：

```bash
cd project/build
./rpc_upstream_client_demo
```

说明：

- upstream 客户端会在进程内配置加权后端（9100 权重大于 9101）
- 返回消息前缀可用于观察命中后端（`from_9100` / `from_9101`）

### 6.5 TinyPB 负载均衡综合示例

该示例在单个程序中启动多个服务端并测试负载均衡：

```bash
cd project/build
./rpc_tinypb_loadbalance_demo
```

特点：

- 启动 3 个服务端（端口 20000, 20001, 20002）
- 配置加权负载均衡（权重 5:3:2）
- 发送 10 个请求，观察分布

## 7. 单机多线程 QPS 测试（Simple RPC）

> 本测试使用 **Simple RPC**（非 TinyPB RPC）进行压测。

该测试程序会在同一进程内：

1. 启动本地单服务端（127.0.0.1:19000）
2. 以线程档位 `[1, 2, 4, 8, 16, 32]` 启动客户端并发调用
3. 统计每个档位的成功数、失败数、耗时、QPS、平均时延

运行：

```bash
cd project/build
./rpc_single_client_server_qps_test 1000
```

其中 `1000` 表示"每线程请求数"，可按机器性能调大，例如：

```bash
./rpc_single_client_server_qps_test 5000
```

输出字段含义：

- `threads`：并发线程数
- `total`：总请求数（threads * requests_per_thread）
- `success` / `fail`：成功/失败请求数
- `elapsed`：本档位总耗时（秒）
- `qps`：每秒成功请求数
- `avg_latency`：平均时延（微秒，按总请求均摊）

### 7.1 实测结果（2026-03-20）

本次测试命令：

```bash
cd project/build
./rpc_single_client_server_qps_test 1000
```

测试说明：

- 本地单机回环地址（127.0.0.1）
- 单 client 进程 + 单 server 实例
- 每线程请求数 `requests_per_thread = 1000`

实测数据如下：

| threads | total | success | fail | elapsed(s) | qps | avg_latency(us) |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1000 | 1000 | 0 | 0.17 | 5969.15 | 167.53 |
| 2 | 2000 | 2000 | 0 | 0.15 | 13018.10 | 76.82 |
| 4 | 4000 | 4000 | 0 | 0.15 | 26076.17 | 38.35 |
| 8 | 8000 | 8000 | 0 | 0.14 | 55697.90 | 17.95 |
| 16 | 16000 | 16000 | 0 | 0.15 | 107716.52 | 9.28 |
| 32 | 32000 | 32000 | 0 | 0.19 | 172362.60 | 5.80 |

观察：

- 各线程档位 `fail=0`，本轮无失败请求。
- QPS 随并发线程增加持续上升，说明在该负载下尚未明显触顶。
- `avg_latency` 随线程增加而下降，主要因为总体吞吐提升带来的均摊效应。

### 7.2 TinyPB RPC QPS 测试

本测试使用 **TinyPB RPC**（基于 protobuf 的完整 RPC 协议）进行压测，可与 Simple RPC 测试对比性能差异。

运行：

```bash
cd project/build
./rpc_tinypb_qps_test 1000
```

其中 `1000` 表示"每线程请求数"，可按机器性能调大。

### 7.3 TinyPB QPS 实测结果（2026-05-11）

```bash
cd project/build
./rpc_tinypb_qps_test 1000
```

测试说明：

- 本地单机回环地址（127.0.0.1:19001）
- 单 client 进程 + 单 TinyPB server 实例
- 每线程请求数 `requests_per_thread = 1000`
- 使用 `TinyPbRpcAsyncChannel` + 信号量实现同步等待

实测数据如下：

| threads | total | success | fail | elapsed(s) | qps | avg_latency(us) |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1000 | 1000 | 0 | 0.14 | 7141.46 | 140.03 |
| 2 | 2000 | 2000 | 0 | 0.14 | 14671.47 | 68.16 |
| 4 | 4000 | 4000 | 0 | 0.13 | 30012.98 | 33.32 |
| 8 | 8000 | 8000 | 0 | 0.14 | 57199.93 | 17.48 |
| 16 | 16000 | 16000 | 0 | 0.16 | 103022.13 | 9.71 |
| 32 | 32000 | 32000 | 0 | 0.22 | 144010.29 | 6.94 |

观察：

- TinyPB RPC 由于完整的协议封装（头部、序列化等），QPS 略低于 Simple RPC
- 32 线程时 QPS 达到约 14.4 万/秒
- `avg_latency` 随线程增加持续下降，表现出良好的并发性能

### 7.4 性能差异分析

**1线程时 TinyPB QPS 反而更高的原因**：

| 因素 | Simple RPC | TinyPB RPC |
|------|-----------|-----------|
| **连接复用** | 每次调用创建新连接 | 同一 channel 复用连接 |
| **同步机制** | `WFFacilities::WaitGroup`（较重） | 自定义轻量 `Semaphore` |
| **Task 创建** | 每次 call 都创建新 task | channel 复用优化 |

**关键实现差异**：

- **Simple RPC** 使用静态方法 `SimpleRpcClient::call()`，每次调用都重新创建 `RpcTask` 和连接
- **TinyPB RPC** 使用 `TinyPbRpcAsyncChannel` 实例，每个线程只创建一次，连接可能被复用

**32线程时性能交叉的原因**：

- 高并发场景下，Simple RPC 的独立连接减少了锁竞争
- Workflow 内部的连接池和负载均衡策略在高并发时更有效

### 7.5 测试公平性说明

当前测试存在一定的实现差异：

1. **Simple RPC 测试**：每次循环调用静态方法，无连接复用
2. **TinyPB RPC 测试**：每个线程复用一个 channel 实例

如需更公平的性能对比，建议：
- 让 Simple RPC 也复用 channel（如果 API 支持）
- 使用相同的同步机制
- 增加预热阶段消除缓存效应

## 8. 如何扩展一个新 RPC 方法

### 8.1 Simple RPC 方式

1. 在 `proto/echo.proto` 中增加消息体定义
2. 重新生成 protobuf 代码并重编译
3. 在服务端使用 `register_method<Req, Resp>(...)` 注册处理器
4. 在客户端通过 `SimpleRpcClient::call(...)` 发起调用
5. 检查返回：`state == WFT_STATE_SUCCESS && status == RPC_OK`

### 8.2 TinyPB RPC 方式

1. 在 `proto/echo.proto` 中增加消息体和 service rpc 定义（确保 `cc_generic_services = true`）
2. 重新生成 protobuf 代码并重编译
3. 编写服务实现类继承生成的 Service 基类
4. 使用 `REGISTER_SERVICE` 宏注册服务
5. 在客户端创建 `Service_Stub` 并调用方法

示例代码：

**服务端**：
```cpp
class EchoServiceImpl : public wf::rpc::example::EchoService {
public:
    void Echo(google::protobuf::RpcController* controller,
              const wf::rpc::example::EchoRequest* request,
              wf::rpc::example::EchoResponse* response,
              google::protobuf::Closure* done) override {
        response->set_message("echo_tinypb: " + request->message());
        if (done)
            done->Run();
    }
};

// 注册服务
REGISTER_SERVICE(EchoServiceImpl);
```

**客户端**：
```cpp
wf_rpc::TinyPbRpcChannel channel("127.0.0.1", 20000);
wf::rpc::example::EchoService_Stub stub(&channel);

wf_rpc::TinyPbRpcController controller;
controller.SetTimeout(5000);

wf::rpc::example::EchoRequest request;
request.set_message("hello_tinypb");

wf::rpc::example::EchoResponse response;
stub.Echo(&controller, &request, &response, nullptr);
```

## 9. 常见问题排查

### 9.1 服务端启动失败

常见原因：

- 端口已被占用
- 没有权限绑定对应端口
- 地址配置错误

建议：

```bash
ss -lntp | grep 9000
ss -lntp | grep 9100
ss -lntp | grep 9101
ss -lntp | grep 19000
ss -lntp | grep 20000
```

### 9.2 客户端调用失败（state/status 非成功）

检查顺序建议：

1. 服务是否已启动且监听正确端口
2. `service` / `method` 字符串是否与服务端注册完全一致
3. Protobuf 请求/响应结构是否匹配
4. 是否误把 upstream 名称配置成可被 DNS 解析的真实域名（可能绕过预期路由）

### 9.3 CMake 新目标找不到

当你新增了可执行目标但 `cmake --build build --target ...` 提示无该 target，通常是因为未重新配置。执行：

```bash
cd project
rm -rf build
mkdir build && cd build
cmake ..
```

然后再次构建目标。

## 10. 开发建议

- 推荐先跑通 `simple`，再切到 `tinypb`，最后使用 `upstream`
- 压测时先用小请求量验证，再逐步增大
- 若要做稳定对比，建议增加预热阶段和多轮统计
- 若要观察尾延迟，建议额外记录 p95/p99
- TinyPB 支持异步调用，适合高并发场景

## 11. 技术架构

### 11.1 整体架构

```
┌─────────────────────────────────────────────────────┐
│                    Application                        │
├─────────────────────────────────────────────────────┤
│  ┌─────────────────┐    ┌────────────────────────┐  │
│  │ SimpleRpcClient  │    │   TinyPbRpcChannel     │  │
│  │ SimpleRpcServer  │    │ TinyPbRpcAsyncChannel  │  │
│  └────────┬────────┘    └───────────┬────────────┘  │
│           │                         │              │
│  ┌────────▼─────────────────────────▼────────────┐  │
│  │              RPC Framework                   │  │
│  │  ┌──────────────┐    ┌────────────────────┐   │  │
│  │  │RpcDispatcher │    │  TinyPbCodec       │   │  │
│  │  └──────────────┘    └────────────────────┘   │  │
│  └───────────────────────┬───────────────────────┘  │
│                          │                           │
│  ┌───────────────────────▼───────────────────────┐   │
│  │           Workflow Network Layer              │   │
│  │    WFServer / WFClient / CommScheduler        │   │
│  └───────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

### 11.2 调用流程（TinyPB）

```
Client                                  Server
  │                                        │
  │  1. stub.Echo()                       │
  │     │                                 │
  │  2. TinyPbRpcChannel::CallMethod()   │
  │     │                                 │
  │  3. Serialize request + encode()      │
  │     │                                 │
  │  4. send() ─────────────────────────►│
  │     │                                 │
  │  5. recv() ◄─────────────────────────│
  │     │                                 │
  │  6. decode()                          │
  │     │                                 │
  │  7. ParseFromString()                 │
  │     │                                 │
  │  8. done->Run()                       │
```

## 12. 许可证与致谢

本仓库包含 Workflow 目录，其许可证与说明见 `workflow/` 内相关文件（如 `LICENSE`、`README.md`、`README_cn.md`）。

---

