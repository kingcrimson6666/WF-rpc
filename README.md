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
├── docker-compose.etcd.yml        # etcd 容器配置（服务注册中心）
├── docker-compose.yml             # 综合场景编排
├── Dockerfile                     # 多阶段构建镜像
├── .dockerignore                  # Docker 构建忽略配置
├── README.md                      # 项目说明文档
├── development_plan.md            # 开发计划文档
├── rpc.md                         # RPC 详细文档
├── proto/
│   └── echo.proto                 # 示例 RPC 协议定义
├── rpc/
│   ├── include/
│   │   └── wf_rpc/
│   │       └── tinypb.h          # TinyPB 公共接口
│   └── src/
│       ├── tinypb_codec.*       # TinyPB 协议编解码器（含 CRC32 校验）
│       ├── tinypb_struct.*      # TinyPB 数据结构
│       ├── tinypb_constants.*   # TinyPB 常量定义
│       ├── tinypb_rpc_server.*  # TinyPB 服务端（支持 TLS）
│       ├── tinypb_rpc_channel.* # TinyPB 同步客户端通道（支持 TLS）
│       ├── tinypb_rpc_async_channel.*  # TinyPB 异步客户端通道
│       ├── tinypb_rpc_controller.*    # RPC 控制器
│       ├── tinypb_rpc_dispatcher.*     # 服务分发器
│       ├── tinypb_rpc_upstream.*       # Upstream 管理
│       ├── rpc_message.*        # RPC 消息编解码
│       ├── rpc_framework.*      # 底层 RPC server/client/upstream 封装
│       ├── rpc_easy.*           # 便捷 API（SimpleRpcServer/Client）
│       ├── rpc_config.*         # XML 配置加载
│       ├── rpc_circuit_breaker.*     # 熔断组件
│       ├── rpc_rate_limiter.*        # 限流组件
│       ├── rpc_metrics.*              # Prometheus 指标监控
│       ├── rpc_logger.*                # 结构化日志系统
│       └── rpc_service_registry.*     # 服务注册中心（etcd 集成）
├── example/
│   ├── simple_server_demo.cc        # Simple RPC 服务端示例
│   ├── simple_client_demo.cc         # Simple RPC 客户端示例
│   ├── upstream_server_demo.cc        # Upstream 双后端示例
│   ├── upstream_client_demo.cc       # Upstream 负载均衡客户端
│   ├── tinypb_server_demo.cc         # TinyPB 服务端示例
│   ├── tinypb_client_demo.cc         # TinyPB 同步客户端示例
│   ├── tinypb_async_client_demo.cc   # TinyPB 异步客户端示例
│   ├── tinypb_loadbalance_demo.cc    # TinyPB 负载均衡综合示例
│   ├── tinypb_tls_server_demo.cc     # TinyPB TLS 服务端示例
│   ├── tinypb_tls_client_demo.cc     # TinyPB TLS 客户端示例
│   ├── tinypb_service_discovery_server_demo.cc  # 服务注册示例
│   ├── tinypb_service_discovery_client_demo.cc  # 服务发现示例
│   └── tinypb_registry_loadbalance_demo.cc      # 服务发现+负载均衡示例
├── test/
│   ├── single_client_server_qps_test.cc  # 多线程 QPS 压测
│   └── tinypb_unittest.cc               # TinyPB 单元测试
├── scripts/
│   ├── build_proto_examples.sh    # make 方式下构建 proto + example 脚本
│   └── generate_cert.sh           # TLS 证书生成脚本
├── conf/
│   ├── server.crt                 # TLS 服务端证书
│   └── server.key                 # TLS 服务端私钥
├── etcd/                          # etcd 二进制（用于服务注册中心）
└── workflow/                      # Workflow 异步框架子模块
```

## 2. 核心能力

### 2.1 两种 RPC 模式对比

| 特性 | Simple RPC | TinyPB RPC |
|------|------------|------------|
| **协议格式** | 自定义头部 + protobuf | TinyPB 二进制协议（CRC32 校验） |
| **服务定义** | 运行时注册 | protoc 生成服务基类 |
| **客户端调用** | `call(host, port, service, method, req, resp)` | `stub->method(controller, req, resp, done)` |
| **服务端注册** | `register_method<Req, Resp>("method", handler)` | `REGISTER_SERVICE(ServiceImpl)` |
| **异步支持** | 不支持 | 支持异步通道 |
| **TLS 支持** | 不支持 | 支持 TLS 1.2+ |
| **适用场景** | 快速原型、简单调用 | 生产级服务 |

### 2.2 TinyPB 协议格式

```
| start(1B=0x02) | pk_len(4B) | msg_req_len(4B) | msg_req(NB) |
| service_name_len(4B) | service_full_name(NB) | err_code(4B) |
| err_info_len(4B) | err_info(NB) | pb_data(NB) | checksum(4B) | end(1B=0x03) |
```

**checksum**：使用 CRC32 算法进行数据完整性校验

### 2.3 RPC 状态语义

`rpc/src/rpc_framework.h` 定义了统一状态码：

- `RPC_OK`：成功
- `RPC_BAD_REQUEST`：非法请求
- `RPC_NOT_FOUND`：找不到 service/method
- `RPC_PROTO_PARSE_ERROR`：请求/响应 protobuf 解析失败
- `RPC_PROTO_SERIALIZE_ERROR`：响应序列化失败
- `RPC_INTERNAL_ERROR`：服务端内部错误
- `RPC_NETWORK_ERROR`：网络层错误
- `RPC_CIRCUIT_BREAKER_OPEN`：熔断触发
- `RPC_RATE_LIMITED`：限流触发

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
| **熔断机制** | `CircuitBreaker` | 自动熔断保护 |
| **限流机制** | `RateLimiter` | 基于 Token Bucket 的限流 |
| **服务注册** | `ServiceRegistry` | etcd 服务注册中心 |

### 2.5 安全性

| 能力 | 说明 |
|------|------|
| **TLS 支持** | 服务端和客户端均支持 TLS 加密传输 |
| **数据完整性** | CRC32 校验确保数据传输完整性 |

### 2.6 可观测性

| 能力 | 说明 |
|------|------|
| **Prometheus 指标** | 暴露 QPS、延迟、错误率、连接数等指标 |
| **结构化日志** | 支持多级别日志（DEBUG/INFO/WARN/ERROR/FATAL） |
| **链路追踪** | 预留 OpenTelemetry 集成接口 |

### 2.7 便捷 API

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
├── registry_enabled_ (是否启用服务注册)
├── service_name_ (服务名称)
└── registerService() / dispatch() / register_to_registry()
```

### 3.2 TinyPB 客户端组件

```
TinyPbRpcChannel (同步通道)
├── host_/port_ 或 url_ (连接信息)
├── use_upstream_ (是否使用 upstream)
├── use_tls_ (是否使用 TLS)
├── cert_file_ / key_file_ (TLS 证书文件)
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

### 3.4 CircuitBreaker（熔断组件）

```
CircuitBreaker
├── STATE_CLOSED：正常状态，允许所有请求
├── STATE_OPEN：熔断状态，拒绝所有请求
├── STATE_HALF_OPEN：半开状态，允许试探性请求
├── failure_threshold_：失败阈值
├── success_threshold_：成功阈值
└── timeout_ms_：熔断超时时间
```

### 3.5 RateLimiter（限流组件）

基于 Token Bucket 算法：

- `set_rate(int qps)`：设置限流速率
- `try_acquire(int permits)`：尝试获取令牌

### 3.6 ServiceRegistry（服务注册中心）

基于 etcd 的服务注册发现：

- `register_service()`：注册服务到 etcd
- `unregister_service()`：从 etcd 注销服务
- `discover()`：发现服务实例
- `watch()`：监听服务变化
- 自动心跳续约（TTL）

## 4. 环境依赖

建议在 Linux 下准备：

- C++ 编译器：`g++`（支持 C++11）
- CMake：`>= 3.16`
- GNU Make
- Protobuf：`protoc` + C++ 库
- OpenSSL 开发库
- pthread / dl / rt（Linux 常见系统库）
- etcd（可选，用于服务注册中心）

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
| `rpc_tinypb_qps_test` | 多线程 QPS 压测（TinyPB RPC） |
| `rpc_tinypb_loadbalance_demo` | TinyPB 负载均衡综合示例 |
| `rpc_tinypb_tls_server_demo` | TinyPB TLS 服务端 |
| `rpc_tinypb_tls_client_demo` | TinyPB TLS 客户端 |
| `rpc_service_discovery_server_demo` | 服务注册服务端 |
| `rpc_service_discovery_client_demo` | 服务发现客户端 |

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
- `docker-compose.etcd.yml`：etcd 服务注册中心
- `.dockerignore`：缩小构建上下文

#### 5.3.1 构建镜像

```bash
cd project
docker build -t workflow-rpc-demo:latest .
```

#### 5.3.2 运行 etcd（服务注册中心）

```bash
cd project
docker-compose -f docker-compose.etcd.yml up -d
```

##### 5.3.3 运行 simple 场景

```bash
cd project
docker compose --profile simple up --build --abort-on-container-exit simple-server simple-client
```

#### 5.3.4 运行 upstream 场景

```bash
cd project
docker compose --profile upstream up --build --abort-on-container-exit upstream-server upstream-client
```

#### 5.3.5 运行 QPS 场景

```bash
cd project
docker compose --profile qps run --build --rm qps-test
```

#### 5.3.6 运行服务注册场景

```bash
cd project
docker-compose -f docker-compose.etcd.yml up -d
docker compose --profile registry up --build --abort-on-container-exit registry-server registry-client
```

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

### 6.3 TinyPB RPC TLS 示例

首先生成证书：

```bash
cd project
./scripts/generate_cert.sh ./conf
```

终端 1（TLS 服务端）：

```bash
cd project/build
./rpc_tinypb_tls_server_demo 127.0.0.1 20001 ./conf/server.crt ./conf/server.key
```

终端 2（TLS 客户端）：

```bash
cd project/build
./rpc_tinypb_tls_client_demo 127.0.0.1 20001 ./conf/server.crt
```

### 6.4 TinyPB RPC 服务注册示例

首先启动 etcd：

```bash
cd project
docker-compose -f docker-compose.etcd.yml up -d
```

终端 1（服务注册服务端）：

```bash
cd project/build
./rpc_service_discovery_server_demo 20000 EchoService
```

终端 2（服务发现客户端）：

```bash
cd project/build
./rpc_service_discovery_client_demo EchoService 127.0.0.1:2379
```

### 6.5 upstream 加权路由示例

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

### 6.6 TinyPB 负载均衡综合示例

```bash
cd project/build
./rpc_tinypb_loadbalance_demo
```

## 7. 单机多线程 QPS 测试

### 7.1 Simple RPC QPS 测试

```bash
cd project/build
./rpc_single_client_server_qps_test 1000
```

### 7.2 TinyPB RPC QPS 测试

```bash
cd project/build
./rpc_tinypb_qps_test 1000
```

### 7.3 实测结果

#### 测试环境
- 测试地址：127.0.0.1（本地回环）
- 测试方法：Echo（简单业务逻辑）
- 测试参数：每个线程发送5000个请求
- 测试线程：1、2、4、8、16、32线程

#### Simple RPC 性能测试结果

| 线程数 | 总请求数 | 成功数 | 失败数 | 耗时(秒) | QPS | 平均延迟(微秒) |
|--------|----------|--------|--------|----------|-----|----------------|
| 1 | 5000 | 5000 | 0 | 0.77 | 6453.32 | 154.96 |
| 2 | 10000 | 10000 | 0 | 0.75 | 13392.93 | 74.67 |
| 4 | 20000 | 20000 | 0 | 0.71 | 28082.21 | 35.61 |
| 8 | 40000 | 40000 | 0 | 0.75 | 53670.52 | 18.63 |
| 16 | 80000 | 80000 | 0 | 0.82 | 97675.19 | 10.24 |
| 32 | 160000 | 160000 | 0 | 1.05 | 152513.80 | 6.56 |

#### TinyPB RPC 性能测试结果

| 线程数 | 总请求数 | 成功数 | 失败数 | 耗时(秒) | QPS | 平均延迟(微秒) |
|--------|----------|--------|--------|----------|-----|----------------|
| 1 | 5000 | 5000 | 0 | 0.78 | 6389.16 | 156.52 |
| 2 | 10000 | 10000 | 0 | 0.75 | 13418.25 | 74.53 |
| 4 | 20000 | 20000 | 0 | 0.70 | 28674.34 | 34.87 |
| 8 | 40000 | 40000 | 0 | 0.72 | 55470.88 | 18.03 |
| 16 | 80000 | 80000 | 0 | 0.77 | 103264.72 | 9.68 |
| 32 | 160000 | 160000 | 0 | 0.99 | 161340.41 | 6.20 |

#### 性能对比表

| 线程数 | Simple RPC QPS | TinyPB RPC QPS | QPS差异 | Simple RPC延迟 | TinyPB RPC延迟 | 延迟差异 |
|--------|----------------|----------------|---------|----------------|----------------|----------|
| 1 | 6453.32 | 6389.16 | -1.0% | 154.96us | 156.52us | +1.0% |
| 2 | 13392.93 | 13418.25 | +0.2% | 74.67us | 74.53us | -0.2% |
| 4 | 28082.21 | 28674.34 | +2.1% | 35.61us | 34.87us | -2.1% |
| 8 | 53670.52 | 55470.88 | +3.3% | 18.63us | 18.03us | -3.3% |
| 16 | 97675.19 | 103264.72 | +5.7% | 10.24us | 9.68us | -5.7% |
| 32 | 152513.80 | 161340.41 | +5.8% | 6.56us | 6.20us | -5.8% |

#### 关键发现

1. **单线程性能**：Simple RPC略好（-1.0%）
   - 协议开销较小
   - 编码/解码较快
   - 无CRC32计算

2. **低并发性能**：两者接近（±0.2%）
   - 差异不明显
   - 可以忽略

3. **中等并发性能**：TinyPB RPC略好（+2.1%到+3.3%）
   - 异步处理优势显现
   - 并发优化效果

4. **高并发性能**：TinyPB RPC明显更好（+5.7%到+5.8%）
   - 异步处理效率更高
   - 并发扩展能力更强

#### 性能分析

**Simple RPC特点：**
- 协议开销较小（约52字节）
- 编码/解码步骤较少
- 无数据完整性验证（CRC32）
- 适合简单业务、低并发场景

**TinyPB RPC特点：**
- 协议开销较大（约54字节）
- 编码/解码步骤较多（含CRC32计算）
- 有数据完整性验证（CRC32）
- 有请求追踪功能（msg_req）
- 适合复杂业务、高并发场景

**性能差异原因：**
- 单线程：Simple RPC略好，因为协议开销较小
- 高并发：TinyPB RPC更好，因为异步处理效率更高

两种RPC都达到了工业级性能水平（QPS 150000+），可以根据具体需求灵活选择。

## 8. 如何扩展一个新 RPC 方法

### 8.1 TinyPB RPC 方式

1. 在 `proto/echo.proto` 中增加消息体和 service rpc 定义
2. 重新生成 protobuf 代码并重编译
3. 编写服务实现类继承生成的 Service 基类
4. 使用 `REGISTER_SERVICE` 宏注册服务
5. 在客户端创建 `Service_Stub` 并调用方法

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

## 9. 服务注册中心使用指南

基于 etcd 的服务注册发现功能，支持：
- 服务注册与注销
- 服务发现（查询所有可用实例）
- 服务监听（实时更新）
- 自动心跳续约（TTL 机制）

### 9.1 服务端注册

```cpp
int main() {
    // 设置服务注册
    wf_rpc::GetRpcServer()->set_registry_enabled(true);
    wf_rpc::GetRpcServer()->set_service_name("EchoService");
    wf_rpc::GetRpcServer()->set_registry_endpoint("127.0.0.1:2379");
    
    // 启动服务时自动注册
    wf_rpc::GetRpcServer()->start(20000);
    
    return 0;
}
```

### 9.2 客户端发现

```cpp
int main() {
    // 设置注册中心地址
    wf_rpc::ServiceRegistry::instance().set_endpoint("127.0.0.1:2379");
    
    // 发现服务
    auto endpoints = wf_rpc::ServiceRegistry::instance().discover("EchoService");
    
    // 使用端点进行调用...
    for (const auto& ep : endpoints) {
        std::cout << "Found endpoint: " << ep.ip << ":" << ep.port << std::endl;
    }
}
```

### 9.3 服务监听

```cpp
int main() {
    wf_rpc::ServiceRegistry::instance().set_endpoint("127.0.0.1:2379");
    
    // 监听服务变化（每5秒更新一次）
    wf_rpc::ServiceRegistry::instance().watch("EchoService", [](const std::vector<ServiceEndpoint>& eps) {
        std::cout << "Service endpoints updated: " << eps.size() << std::endl;
        for (const auto& ep : eps) {
            std::cout << "  - " << ep.ip << ":" << ep.port << std::endl;
        }
    });
    
    // 保持运行...
    while (true) sleep(1);
}
```

### 9.4 etcd 安装与运行

**方式一：使用 Docker**

```bash
docker-compose -f docker-compose.etcd.yml up -d
```

**方式二：本地二进制**

```bash
# 进入 etcd 目录
cd etcd

# 启动 etcd（单机模式）
./etcd --listen-client-urls=http://127.0.0.1:2379 --advertise-client-urls=http://127.0.0.1:2379
```

### 9.5 服务注册流程

1. 服务端启动时，自动创建 etcd lease（默认 TTL 30秒）
2. 将服务信息（IP:Port）注册到 etcd，key 格式：`/wf_rpc/services/{service_name}/{ip}:{port}`
3. 后台线程定期发送心跳（每 10 秒）维持 lease
4. 客户端通过前缀查询发现所有服务实例
5. 服务端关闭时自动注销服务并释放 lease

## 10. 熔断与限流使用

### 10.1 熔断

```cpp
wf_rpc::CircuitBreaker cb("EchoService", 50, 5, 30000);

if (cb.allow_request()) {
    // 执行请求
    if (success) {
        cb.record_success();
    } else {
        cb.record_failure();
    }
} else {
    // 熔断触发，拒绝请求
}
```

### 10.2 限流

```cpp
wf_rpc::RateLimiter limiter("EchoService", 1000);

if (limiter.try_acquire()) {
    // 执行请求
} else {
    // 限流触发
}
```

## 11. 常见问题排查

### 11.1 服务端启动失败

- 端口已被占用
- 没有权限绑定对应端口
- 地址配置错误

### 11.2 客户端调用失败

检查顺序：
1. 服务是否已启动且监听正确端口
2. `service` / `method` 字符串是否与服务端注册完全一致
3. Protobuf 请求/响应结构是否匹配
4. TLS 证书配置是否正确

### 11.3 服务注册失败

检查：
1. etcd 是否正常运行
2. etcd 地址配置是否正确
3. 网络连接是否正常

## 12. 技术架构

### 12.1 整体架构

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
│  │  │CircuitBreaker│    │   RateLimiter      │   │  │
│  │  └──────────────┘    └────────────────────┘   │  │
│  └───────────────────────┬───────────────────────┘  │
│                          │                           │
│  ┌───────────────────────▼───────────────────────┐   │
│  │           ServiceRegistry (etcd)             │   │
│  └───────────────────────┬───────────────────────┘   │
│                          │                           │
│  ┌───────────────────────▼───────────────────────┐   │
│  │           Workflow Network Layer              │   │
│  │    WFServer / WFClient / CommScheduler        │   │
│  └───────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

## 13. 许可证与致谢

本仓库包含 Workflow 目录，其许可证与说明见 `workflow/` 内相关文件。

---